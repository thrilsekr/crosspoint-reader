#include "LocalGetTestActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>
#include <Bitmap.h>
#include <HalStorage.h>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/reader/AiScreensaverStorage.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/AiScreensaverService.h"
#include "network/HttpDownloader.h"

void LocalGetTestActivity::onEnter() {
  Activity::onEnter();

  if (WiFi.status() == WL_CONNECTED) {
    promptForUrl();
    return;
  }

  // Preserve the proven CrossPoint connection flow. The worker service then
  // reuses the station connection established by WifiSelectionActivity.
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             {
                               RenderLock lock(*this);
                               wifiCancelled = true;
                               statusMessage = "Wi-Fi connection cancelled";
                               state = State::RESULT;
                             }
                             requestUpdate(true);
                             return;
                           }
                           promptForUrl();
                         });
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
    waitingForGet = true;
  }
  requestUpdate(true);
  performGet();
}

void LocalGetTestActivity::performGet() {
  if (!aiScreensaverService.startGet(requestUrl)) {
    waitingForGet = false;
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

  // Keep the incoming file separate from the active image until its BMP
  // headers parse successfully. The global /sleep.bmp override is untouched.
  constexpr char BMP_URL[] = "http://192.168.1.177:8765/test.bmp";
  const std::string imageDirectory = aiScreensaverDirectoryForCachePath(bookCachePath);
  bool saved = false;
  if (!imageDirectory.empty() && Storage.ensureDirectoryExists(imageDirectory.c_str())) {
    const std::string incomingPath = imageDirectory + "/incoming.bmp";
    const std::string currentPath = imageDirectory + "/current.bmp";
    const std::string previousPath = imageDirectory + "/previous.bmp";
    const auto result = HttpDownloader::downloadToFile(BMP_URL, incomingPath);
    const int httpStatus = HttpDownloader::lastStatus();
    bool validBmp = false;
    if (result == HttpDownloader::OK) {
      HalFile incomingFile;
      if (Storage.openFileForRead("X4BMP", incomingPath, incomingFile)) {
        Bitmap bitmap(incomingFile);
        validBmp = incomingFile.fileSize() >= 54 && bitmap.parseHeaders() == BmpReaderError::Ok;
      }
    }

    if (validBmp) {
      Storage.remove(previousPath.c_str());
      const bool hadCurrent = Storage.exists(currentPath.c_str());
      const bool backedUp = !hadCurrent || Storage.rename(currentPath.c_str(), previousPath.c_str());
      if (backedUp) {
        saved = Storage.rename(incomingPath.c_str(), currentPath.c_str());
        if (!saved && hadCurrent) Storage.rename(previousPath.c_str(), currentPath.c_str());
      }
    }
    {
      RenderLock lock(*this);
      bmpSucceeded = saved;
      bmpHttpStatus = httpStatus;
      statusMessage = saved ? "BMP saved for this book" : "BMP download/validation failed";
      state = State::RESULT;
    }
  } else {
    RenderLock lock(*this);
    bmpSucceeded = false;
    bmpHttpStatus = HttpDownloader::lastStatus();
    statusMessage = "Could not create book image folder";
    state = State::RESULT;
  }
  requestUpdate(true);
}

void LocalGetTestActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (state == State::REQUESTING && waitingForGet) {
    const auto result = aiScreensaverService.snapshot();
    if (result.complete) {
      waitingForGet = false;
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
    if (wifiCancelled || bmpAttempted) {
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
  if (!aiScreensaverService.isIdle()) return false;
  aiScreensaverService.releaseOwnedWifi();
  return true;
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

  const std::string fileLine =
      !bmpAttempted ? "BMP file: NOT TESTED" : (bmpSucceeded ? "BMP file: SAVED" : "BMP file: NOT SAVED");
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY + 90, fileLine.c_str());
  if (state == State::REQUESTING || state == State::RESULT) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY + 120, statusMessage.c_str());
  }

  if (state == State::RESULT) {
    const auto labels = (wifiCancelled || bmpAttempted) ? mappedInput.mapLabels("Back", "Done", "", "")
                        : postAttempted                 ? mappedInput.mapLabels("Back", "Download BMP", "", "")
                                                        : mappedInput.mapLabels("Back", "POST test", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}
