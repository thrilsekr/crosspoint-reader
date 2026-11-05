#include "LocalGetTestActivity.h"

#include <GfxRenderer.h>
#include "MappedInputManager.h"
#include "network/AiScreensaverService.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "components/UITheme.h"

void LocalGetTestActivity::onEnter() {
  Activity::onEnter();
  // The application service reuses connected Wi-Fi or joins the saved station
  // network without requiring a Wi-Fi selection sub-activity.
  promptForUrl();
}

void LocalGetTestActivity::promptForUrl() {
  // Temporary local diagnostic endpoint. This IPv4 literal avoids DNS and
  // remains on the user's LAN; it is never sent to an external service.
  requestUrl = "http://192.168.1.177:8765/event";
  {
    RenderLock lock(*this);
    state = State::REQUESTING;
    statusMessage = "Starting HTTP GET...";
    getAttempted = true;
  }
  requestUpdate(true);
  performGet();
}

void LocalGetTestActivity::performGet() {
  if (!aiScreensaverService.startGet(requestUrl)) {
    const auto result = aiScreensaverService.snapshot();
    RenderLock lock(*this);
    getSucceeded = result.success;
    getHttpStatus = result.httpStatus;
    statusMessage = result.complete ? result.message : "Could not start GET";
    state = State::RESULT;
    requestUpdate(true);
  }
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
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (state == State::REQUESTING && getAttempted) {
    const auto result = aiScreensaverService.snapshot();
    if (result.complete) {
      {
        RenderLock lock(*this);
        getSucceeded = result.success;
        getHttpStatus = result.httpStatus;
        statusMessage = result.message;
        state = State::RESULT;
      }
      requestUpdate(true);
    }
  }

  if (state != State::RESULT) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (bmpAttempted) {
      finish();
    } else if (!postAttempted) {
      performPost();
    } else {
      performBmpDownload();
    }
  }
}

void LocalGetTestActivity::prepareForSleep() { aiScreensaverService.requestCancel(); }

bool LocalGetTestActivity::prepareToExit() {
  aiScreensaverService.requestCancel();
  return aiScreensaverService.isIdle();
}

void LocalGetTestActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  GUI.drawHeader(renderer, UITheme::getInstance().getHeaderRect(renderer, true, false), "Local Network Diagnostic");

  const int centerY = screen.y + screen.height / 2 - 50;
  const auto stageLine = [this](const char* label, const bool attempted, const bool succeeded, const int status) {
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
