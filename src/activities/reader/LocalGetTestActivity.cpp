#include "LocalGetTestActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
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
                               wifiCancelled = true;
                               state = State::RESULT;
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
    statusMessage = "Sending HTTP GET...";
    getAttempted = true;
  }
  requestUpdate(true);

  const bool success = HttpDownloader::fetchUrl(
      requestUrl, [](const uint8_t*, const size_t) { return true; });

  const int httpStatus = HttpDownloader::lastStatus();
  {
    RenderLock lock(*this);
    getSucceeded = success;
    getHttpStatus = httpStatus;
    statusMessage = success ? "GET test complete" : "GET test failed";
    state = State::RESULT;
  }
  requestUpdate(true);
}

void LocalGetTestActivity::performPost() {
  {
    RenderLock lock(*this);
    state = State::REQUESTING;
    statusMessage = "Sending test POST...";
    postAttempted = true;
  }
  requestUpdate(true);

  std::string response;
  int httpStatus = 0;
  const bool success = HttpDownloader::postForm(requestUrl, "test=x4-post-test", response, &httpStatus);
  {
    RenderLock lock(*this);
    postSucceeded = success;
    postHttpStatus = httpStatus;
    statusMessage = success ? "POST test complete" : "POST test failed";
    state = State::RESULT;
  }
  requestUpdate(true);
}

void LocalGetTestActivity::performBmpDownload() {
  {
    RenderLock lock(*this);
    state = State::REQUESTING;
    statusMessage = "Downloading test BMP...";
    bmpAttempted = true;
  }
  requestUpdate(true);

  // Fixed local diagnostic endpoint. The file is deliberately kept separate
  // from /sleep.bmp so this test cannot change the active screensaver.
  constexpr char BMP_URL[] = "http://192.168.1.177:8765/test.bmp";
  constexpr char DEST_PATH[] = "/x4-download-test.bmp";
  const auto result = HttpDownloader::downloadToFile(BMP_URL, DEST_PATH);
  const int httpStatus = HttpDownloader::lastStatus();
  {
    RenderLock lock(*this);
    bmpSucceeded = result == HttpDownloader::OK;
    bmpHttpStatus = httpStatus;
    statusMessage = bmpSucceeded ? "BMP download complete" : "BMP download failed";
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
    if (wifiCancelled || bmpAttempted) {
      finish();
    } else if (!postAttempted) {
      performPost();
    } else {
      performBmpDownload();
    }
  }
}

void LocalGetTestActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  GUI.drawHeader(renderer, UITheme::getInstance().getHeaderRect(renderer, true, false), "Local Network Diagnostic");

  const int centerY = screen.y + screen.height / 2 - 50;
  const auto stageLine = [this](const char* label, const bool attempted, const bool succeeded, const int status) {
    if (wifiCancelled) return std::string(label) + ": SKIPPED";
    if (!attempted) return std::string(label) + ": NOT RUN";
    return std::string(label) + (succeeded ? ": PASS HTTP " : ": FAIL HTTP ") + std::to_string(status);
  };

  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY,
                            stageLine("GET /event", getAttempted, getSucceeded, getHttpStatus).c_str());
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY + 30,
                            stageLine("POST /event", postAttempted, postSucceeded, postHttpStatus).c_str());
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY + 60,
                            stageLine("GET /test.bmp", bmpAttempted, bmpSucceeded, bmpHttpStatus).c_str());

  const std::string fileLine = !bmpAttempted ? "BMP file: NOT TESTED"
                              : (bmpSucceeded ? "BMP file: SAVED" : "BMP file: NOT SAVED");
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY + 90, fileLine.c_str());
  if (state == State::REQUESTING) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY + 120, statusMessage.c_str());
  }

  if (state == State::RESULT) {
    const auto labels = (wifiCancelled || bmpAttempted)
                            ? mappedInput.mapLabels("Back", "Done", "", "")
                        : postAttempted ? mappedInput.mapLabels("Back", "Download BMP", "", "")
                                        : mappedInput.mapLabels("Back", "POST test", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}
