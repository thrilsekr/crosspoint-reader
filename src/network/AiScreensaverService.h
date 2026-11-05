#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

class AiScreensaverService {
 public:
  struct Snapshot {
    bool busy = false;
    bool complete = false;
    bool success = false;
    bool cancelled = false;
    int httpStatus = 0;
    std::string message;
  };

  // Called only from the Arduino/main task. The first version deliberately
  // accepts only a local plain-HTTP URL with an IPv4 literal host.
  bool startGet(const std::string& url);
  void loop();
  void requestCancel();
  void releaseOwnedWifi();

  // Main-task only. Called after ActivityManager::prepareForSleep() has
  // released RenderLock and before any sleep-event network delivery.
  void waitUntilIdle();

  bool isIdle() const;
  Snapshot snapshot() const;

 private:
  enum class State { IDLE, CONNECTING, RUNNING, RESULT };

  static void workerTrampoline(void* context);
  void workerMain();
  bool startWorker();
  void finishOnMainTask(bool success, bool cancelled, int httpStatus, const std::string& message);
  void cleanupOwnedWifi();
  static bool isLocalIpv4HttpUrl(const std::string& url);

  // All lifecycle and Wi-Fi fields below are owned by the Arduino/main task.
  State state_ = State::IDLE;
  bool busy_ = false;
  bool ownsWifi_ = false;
  unsigned long connectDeadline_ = 0;
  TaskHandle_t workerTask_ = nullptr;
  std::atomic<bool> cancelRequested_{false};
  std::atomic<bool> workerFinished_{false};
  std::string requestUrl_;

  // The worker publishes its response under this mutex. The main task exposes
  // a completed snapshot only after it has reaped the suspended worker and
  // released any Wi-Fi connection it established.
  mutable std::mutex resultMutex_;
  Snapshot result_;
};

extern AiScreensaverService aiScreensaverService;
