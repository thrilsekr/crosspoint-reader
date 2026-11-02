#pragma once

#include <string>

#include "activities/Activity.h"

/**
 * Temporary local-network diagnostic. Connects using CrossPoint's existing
 * Wi-Fi selector, asks for a local HTTP URL, performs one GET, then allows
 * the user to send a small form POST and inspect its response.
 */
class LocalGetTestActivity final : public Activity {
 public:
  LocalGetTestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("LocalGetTest", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State { CONNECTING, ENTERING_URL, REQUESTING, RESULT };
  State state = State::CONNECTING;
  std::string statusMessage = "Connecting to Wi-Fi...";
  std::string responsePreview;
  std::string requestUrl;
  bool postAttempted = false;

  void promptForUrl();
  void performGet();
  void performPost();
};
