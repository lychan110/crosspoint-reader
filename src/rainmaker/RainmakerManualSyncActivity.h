#pragma once

#include "activities/Activity.h"

namespace rainmaker {

// User-facing "Sync Rainmaker dashboard" activity. Drives RainmakerSyncService
// and renders progress / outcome. Press Back to dismiss.
class RainmakerManualSyncActivity final : public Activity {
 public:
  explicit RainmakerManualSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RainmakerManualSync", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum class State { Idle, Running, DoneOk, DoneOkNoChange, DoneFailed };

  State state = State::Idle;
  char statusMessage[64] = {};

  void goBack() { finish(); }
  void runSync();
};

}  // namespace rainmaker
