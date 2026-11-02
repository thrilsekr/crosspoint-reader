#include "LocalGetTestActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <algorithm>

#include "ActivityResult.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "network/WifiPowerSaveGuard.h"
#include "components/UITheme.h"

void LocalGetTestActivity::onEnter() {
  Activity::onEnter();

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setSleep(false);
    promptForUrl();
    return;
  }

  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             statusMessage = "Wi-Fi connection cancelled";
                             state = State::RESULT;
                             requestUpdate(true);
                             return;
                           }
                           WiFi.setSleep(false);
                           promptForUrl();
                         });
}

void LocalGetTestActivity::promptForUrl() {
  state = State::ENTERING_URL;
  statusMessage = "Enter your PC's local HTTP URL";
  requestUpdate(true);

  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, "Local GET URL", "http://", 96, InputType::Url),
      [this](const ActivityResult& result) {
        if (result.isCancelled) {
          statusMessage = "Test cancelled";
          state = State::RESULT;
          requestUpdate(true);
          return;
        }
        requestUrl = std::get<KeyboardResult>(result.data).text;
        if (requestUrl.empty() || requestUrl == "http://") {
          statusMessage = "No URL entered";
          state = State::RESULT;
          requestUpdate(true);
          return;
        }
        performGet();
      });
}

void LocalGetTestActivity::performGet() {
  state = State::REQUESTING;
  statusMessage = "Sending one HTTP GET...";
  responsePreview.clear();
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
  statusMessage = success ? ("GET succeeded (HTTP " + std::to_string(httpStatus) + ")")
                          : ("GET failed (HTTP " + std::to_string(httpStatus) + ")");
  state = State::RESULT;
  requestUpdate(true);
}

void LocalGetTestActivity::loop() {
  if (state != State::RESULT) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void LocalGetTestActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  GUI.drawHeader(renderer, UITheme::getInstance().getHeaderRect(renderer, true, false), "Local GET Test");

  const int centerY = screen.y + screen.height / 2 - 24;
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY, statusMessage.c_str(), true,
                            EpdFontFamily::BOLD);
  if (state == State::RESULT && !responsePreview.empty()) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, centerY + 36, responsePreview.c_str());
  }

  if (state == State::RESULT) {
    const auto labels = mappedInput.mapLabels("Back", "Done", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}
