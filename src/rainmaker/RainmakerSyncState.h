#pragma once

#include <cstdint>

namespace rainmaker {

// Runtime state persisted at /.crosspoint/rainmaker/state.json. Kept separate
// from user-facing settings so the SD card layout is well-bounded.
struct RainmakerSyncState {
  char lastSha256[65];
  uint32_t sequence;
  char updatedAt[32];
  int16_t sunriseLocalMinutes;  // -1 = absent
  int16_t sunsetLocalMinutes;   // -1 = absent
  uint32_t lastAttemptEpoch;
  uint32_t lastSuccessEpoch;
  char lastError[64];

  static constexpr const char* STATE_DIR = "/.crosspoint/rainmaker";
  static constexpr const char* STATE_FILE = "/.crosspoint/rainmaker/state.json";
  static constexpr const char* CACHE_BMP = "/.crosspoint/rainmaker/latest.bmp";
  static constexpr const char* CACHE_TMP = "/.crosspoint/rainmaker/latest.tmp";

  void clear();
};

// Load the state from STATE_FILE. Returns true if a state was loaded; false
// if the file is missing or invalid (in which case `state` is reset to
// cleared/default values).
bool loadState(RainmakerSyncState& state);

// Persist `state` to STATE_FILE. Atomic write via Storage::writeFile (which
// overwrites the existing file). Returns true on success.
bool saveState(const RainmakerSyncState& state);

// True if a cached dashboard BMP exists on disk at the canonical path.
bool hasCachedDashboard();

// Canonical cache path. Returns CACHE_BMP.
const char* cachedDashboardPath();

}  // namespace rainmaker
