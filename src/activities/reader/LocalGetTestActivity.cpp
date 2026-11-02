#include "LocalGetTestActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "network/WifiPowerSaveGuard.h"
#include "components/UITheme.h"

void LocalGetTestActivity::onEnter() {
  Activity::onEnter();

  if (WiFi.status() == WL_CONNECTED) {
    promptForUrl();
    return;
  }

  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             {
                               RenderLock lock(*this);
                               statusMessage = "Wi-Fi connection cancelled";
                               state = State::RESULT;
                               postAttempted = true;
                             }
                             requestUpdate(true);
                             return;
                           }
                           promptForUrl();
                         });
}

void LocalGetTestActivity::promptForUrl() {
  // Temporary diagnostic endpoint on the user's local PC.
  // Keep this local-only; no address is sent to any external service.
  requestUrl = "http://192.168.1.177:8765/event";
  {
    RenderLock lock(*this);
    state = State::REQUESTING;
    statusMessage = "Using saved local PC endpoint...";
  }
  requestUpdate(true);
  performGet();
}

void LocalGetTestActivity::performGet() {
  {
    RenderLock lock(*this);
    state = State::REQUESTING;
    statusMessage = "Sending one HTTP GET...";
    responsePreview.clear();
    postAttempted = false;
  }
  requestUpdate(true);

  WifiPowerSaveGuard powerSaveGuard;
  const bool success = HttpDownloader::fetchUrl(
      requestUrl,
      [this](const uint8_t* data, const size_t length) {
        constexpr size_t MAX_PREVIEW = 240;
        if (responsePreview.size() < MAX_PREVIEW) {
          const size_t take = std::min(length, MAX_PREVIEW - responsePreview.size());
          responsePreview.append(reinterpret_cast<const char*>(data), take);
        }
        return true;
      });

  const int httpStatus = HttpDownloader::lastStatus();
  {
    RenderLock lock(*this);
    statusMessage = success ? ("GET succeeded (HTTP " + std::to_string(httpStatus) + ")")
                            : ("GET failed (HTTP " + std::to_string(httpStatus) + ")");
    state = State::RESULT;
  }
  requestUpdate(true);
}

void LocalGetTestActivity::performPost() {
  {
    RenderLock lock(*this);
    state = State::REQUESTING;
    statusMessage = "Sending one test POST...";
    responsePreview.clear();
    postAttempted = true;
  }
  requestUpdate(true);

  std::string response;
  int httpStatus = 0;
  const bool success = HttpDownloader::postForm(requestUrl, "test=x4-post-test", response, &httpStatus);

  constexpr size_t MAX_PREVIEW = 240;
  if (response.size() > MAX_PREVIEW) response.resize(MAX_PREVIEW);
  {
    RenderLock lock(*this);
    responsePreview = response;
    statusMessage = success ? ("POST succeeded (HTTP " + std::to_string(httpStatus) + ")")
                            : ("POST failed (HTTP " + std::to_string(httpStatus) + ")");
    state = State::RESULT;
  }
  requestUpdate(true);
}

void LocalGetTestActivity::loop() {
  if (state != State::RESULT) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!postAttempted && !requestUrl.empty()) {
      performPost();
    } else {
      finish();
    }
  }
}

void LocalGetTestActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  GUI.drawHeader(renderer, UITheme::getInstance().getHeaderRect(renderer, true, false), "Local GET/POST Test");

  const int centerY = screen.y + screen.height / 2 - 24;
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY, statusMessage.c_str(), true,
                            EpdFontFamily::BOLD);
  if (state == State::RESULT && !responsePreview.empty()) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY + 36, responsePreview.c_str());
  }

  if (state == State::RESULT) {
    const auto labels = postAttempted ? mappedInput.mapLabels("Back", "Done", "", "")
                                      : mappedInput.mapLabels("Back", "POST test", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}
