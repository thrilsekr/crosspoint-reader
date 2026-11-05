#include "AiScreensaverService.h"

#include <Arduino.h>
#include <Logging.h>
#include <WiFi.h>

#include "WifiCredentialStore.h"
#include "network/HttpDownloader.h"

namespace {
constexpr uint32_t LOCAL_HTTP_TIMEOUT_MS = 5000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 45000;
constexpr uint32_t WORKER_STACK_BYTES = 8192;
constexpr UBaseType_t WORKER_PRIORITY = 1;
}  // namespace

AiScreensaverService aiScreensaverService;

bool AiScreensaverService::isLocalIpv4HttpUrl(const std::string& url) {
  constexpr char prefix[] = "http://";
  if (url.compare(0, sizeof(prefix) - 1, prefix) != 0) return false;

  const size_t hostStart = sizeof(prefix) - 1;
  const size_t hostEnd = url.find_first_of("/:?#", hostStart);
  if (hostEnd == std::string::npos || hostEnd == hostStart) return false;

  const std::string host = url.substr(hostStart, hostEnd - hostStart);
  IPAddress address;
  if (!address.fromString(host.c_str())) return false;

  const uint8_t first = address[0];
  const uint8_t second = address[1];
  return first == 10 || (first == 172 && second >= 16 && second <= 31) || (first == 192 && second == 168) ||
         (first == 169 && second == 254);
}

bool AiScreensaverService::startGet(const std::string& url) {
  // startGet() and every other lifecycle method are called on the Arduino task.
  if (busy_ || workerTask_) return false;

  {
    std::lock_guard<std::mutex> lock(resultMutex_);
    result_ = {};
  }
  cancelRequested_.store(false, std::memory_order_release);
  workerFinished_.store(false, std::memory_order_release);
  requestUrl_ = url;

  if (!isLocalIpv4HttpUrl(requestUrl_)) {
    finishOnMainTask(false, false, 0, "Only local IPv4 HTTP endpoints are supported");
    return false;
  }

  busy_ = true;
  if (WiFi.status() == WL_CONNECTED) {
    // Reuse the existing station connection; this service does not own it.
    state_ = State::RUNNING;
    if (!startWorker()) {
      cleanupOwnedWifi();
      finishOnMainTask(false, false, 0, "Could not start network worker");
      return false;
    }
    return true;
  }

  // Only manage Wi-Fi when it is off or already in STA-only mode. Never
  // change an AP or AP+STA configuration owned by another feature.
  const wifi_mode_t currentMode = WiFi.getMode();
  if (currentMode != WIFI_MODE_NULL && currentMode != WIFI_MODE_STA) {
    finishOnMainTask(false, false, 0, "Wi-Fi AP mode is active");
    return false;
  }

  // Credential loading and all Wi-Fi mode/connection changes stay on the
  // Arduino task. Copy the strings before launching any worker.
  WIFI_STORE.loadFromFile();
  const std::string ssid = WIFI_STORE.getLastConnectedSsid();
  const auto credential = WIFI_STORE.findCredential(ssid);
  if (!credential || credential->ssid.empty()) {
    finishOnMainTask(false, false, 0, "No saved Wi-Fi network is available");
    return false;
  }

  if (currentMode == WIFI_MODE_NULL && !WiFi.mode(WIFI_STA)) {
    finishOnMainTask(false, false, 0, "Could not enable Wi-Fi station mode");
    return false;
  }
  // An enabled but disconnected STA can be left behind after the Wi-Fi
  // selection screen. Re-issue begin() with the saved network instead of
  // failing immediately; the service owns this reconnect until cleanup.
  WiFi.begin(credential->ssid.c_str(), credential->password.c_str());
  ownsWifi_ = true;
  state_ = State::CONNECTING;
  connectDeadline_ = millis() + WIFI_CONNECT_TIMEOUT_MS;
  return true;
}

void AiScreensaverService::loop() {
  if (!busy_) return;

  if (state_ == State::CONNECTING) {
    if (cancelRequested_.load(std::memory_order_acquire)) {
      cleanupOwnedWifi();
      finishOnMainTask(false, true, 0, "Wi-Fi connection cancelled");
      return;
    }

    if (WiFi.status() == WL_CONNECTED) {
      state_ = State::RUNNING;
      if (!startWorker()) {
        cleanupOwnedWifi();
        finishOnMainTask(false, false, 0, "Could not start network worker");
      }
      return;
    }

    if (static_cast<int32_t>(millis() - connectDeadline_) >= 0) {
      cleanupOwnedWifi();
      finishOnMainTask(false, false, 0, "Wi-Fi connection timed out");
    }
    return;
  }

  if (state_ != State::RUNNING || !workerTask_ || !workerFinished_.load(std::memory_order_acquire)) {
    return;
  }

  // The worker has unwound HttpDownloader and WifiPowerSaveGuard, published
  // its result, and suspended itself. Reap only after it is no longer running.
  if (eTaskGetState(workerTask_) != eSuspended) return;
  vTaskDelete(workerTask_);
  workerTask_ = nullptr;

  busy_ = false;
  state_ = State::RESULT;
  {
    std::lock_guard<std::mutex> lock(resultMutex_);
    result_.busy = false;
    result_.complete = true;
  }
}

bool AiScreensaverService::startWorker() {
  workerFinished_.store(false, std::memory_order_release);
  const BaseType_t created = xTaskCreate(&AiScreensaverService::workerTrampoline, "AiHttpGet", WORKER_STACK_BYTES, this,
                                         WORKER_PRIORITY, &workerTask_);
  if (created != pdPASS) {
    workerTask_ = nullptr;
    return false;
  }
  return true;
}

void AiScreensaverService::workerTrampoline(void* context) {
  static_cast<AiScreensaverService*>(context)->workerMain();
}

void AiScreensaverService::workerMain() {
  int httpStatus = 0;

  HttpDownloader::FetchOptions options;
  options.statusOut = &httpStatus;
  options.timeoutMs = LOCAL_HTTP_TIMEOUT_MS;
  options.cancelFlag = &cancelRequested_;
  options.followRedirects = false;

  const bool success = HttpDownloader::fetchUrl(requestUrl_, [](const uint8_t*, size_t) { return true; }, options);

  const bool cancelled = cancelRequested_.load(std::memory_order_acquire) && !success;
  {
    std::lock_guard<std::mutex> lock(resultMutex_);
    result_.success = success;
    result_.cancelled = cancelled;
    result_.httpStatus = httpStatus;
    result_.message = cancelled ? "GET cancelled" : success ? "GET complete" : "GET failed";
  }

  const UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(nullptr);
  LOG_DBG("AI-WORKER", "GET finished: success=%d cancelled=%d status=%d stack high-water=%u", success ? 1 : 0,
          cancelled ? 1 : 0, httpStatus, static_cast<unsigned>(stackHighWater));

  workerFinished_.store(true, std::memory_order_release);
  // The main task reaps this suspended task only after confirming it is no
  // longer executing. All HTTP and Wi-Fi guard cleanup has already completed.
  vTaskSuspend(nullptr);
  vTaskDelete(nullptr);  // Defensive fallback; normally never resumed.
}

void AiScreensaverService::requestCancel() {
  if (busy_) cancelRequested_.store(true, std::memory_order_release);
}

void AiScreensaverService::waitUntilIdle() {
  // Main-task only, and must never be called while holding RenderLock.
  requestCancel();
  while (busy_) {
    loop();
    delay(10);
  }
  cleanupOwnedWifi();
}

void AiScreensaverService::releaseOwnedWifi() {
  // Main-task only. Keep the connection available to sequential diagnostic
  // requests until the owning activity exits or the device begins sleeping.
  if (!busy_) cleanupOwnedWifi();
}

bool AiScreensaverService::isIdle() const { return !busy_; }

AiScreensaverService::Snapshot AiScreensaverService::snapshot() const {
  std::lock_guard<std::mutex> lock(resultMutex_);
  Snapshot copy = result_;
  copy.busy = busy_;
  return copy;
}

void AiScreensaverService::finishOnMainTask(const bool success, const bool cancelled, const int httpStatus,
                                            const std::string& message) {
  busy_ = false;
  state_ = State::RESULT;
  std::lock_guard<std::mutex> lock(resultMutex_);
  result_.busy = false;
  result_.complete = true;
  result_.success = success;
  result_.cancelled = cancelled;
  result_.httpStatus = httpStatus;
  result_.message = message;
}

void AiScreensaverService::cleanupOwnedWifi() {
  if (!ownsWifi_) return;
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  ownsWifi_ = false;
}
