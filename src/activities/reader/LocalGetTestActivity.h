#pragma once

#include <string>

#include "activities/Activity.h"

/**
 * Temporary local-network diagnostic. Performs GET, POST, and BMP download
 * tests sequentially and retains every result on the final screen.
 */
class LocalGetTestActivity final : public Activity {
 public:
  LocalGetTestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("LocalGetTest", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  void prepareForSleep() override;
  bool prepareToExit() override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { REQUESTING, RESULT };
  State state = State::RESULT;
  std::string statusMessage = "Starting HTTP GET...";
  std::string requestUrl;

  bool getAttempted = false;
  bool getSucceeded = false;
  int getHttpStatus = 0;
  bool postAttempted = false;
  bool postSucceeded = false;
  int postHttpStatus = 0;
  bool bmpAttempted = false;
  bool bmpSucceeded = false;
  int bmpHttpStatus = 0;

  void promptForUrl();
  void performGet();
  void performPost();
  void performBmpDownload();
};
