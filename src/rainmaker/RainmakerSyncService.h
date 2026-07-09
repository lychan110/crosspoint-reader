#pragma once

#include <cstdint>

namespace rainmaker {

enum class RainmakerSyncMode : uint8_t {
  Scheduled = 0,
  Manual = 1,
};

enum class RainmakerSyncStatus : uint8_t {
  Ok = 0,
  Disabled,
  MissingConfig,
  LowBattery,
  WifiFailed,
  ManifestFetchFailed,
  ManifestInvalid,
  DownloadFailed,
  HashMismatch,
  FileError,
};

struct RainmakerSyncResult {
  RainmakerSyncStatus status = RainmakerSyncStatus::Ok;
  bool changed = false;          // true if a new BMP replaced the cache
  uint32_t bytesDownloaded = 0;  // bytes written to the BMP cache on success
  char message[96];              // short human-readable reason
};

// Run a sync. In Scheduled mode, sync() is a no-op when the user has
// disabled sync, when configuration is missing, or when the battery is below
// the configured threshold. In Manual mode, those guards are not enforced; the
// caller (UI) is expected to confirm with the user when needed.
//
// Both modes:
//   - connect to the last-used WiFi (or fail with WifiFailed if no
//     credentials are saved)
//   - fetch and validate the manifest
//   - skip the BMP download if the manifest's sha256 matches the cached
//     dashboard
//   - download to a temp file, verify byte count + sha256, then atomically
//     rename to the canonical cache path
//   - persist the resulting state
//
// Never panics, never throws. Always returns a structured result; on failure,
// the existing cache (if any) is left untouched.
RainmakerSyncResult sync(RainmakerSyncMode mode);

}  // namespace rainmaker
