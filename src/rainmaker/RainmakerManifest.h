#pragma once

#include <cstddef>
#include <cstdint>

namespace rainmaker {

// Decoded Rainmaker manifest. Fixed-size character buffers keep the parser
// allocation-free; the parser bounds-checks every string before copying.
struct RainmakerManifest {
  uint8_t version = 0;
  char id[64];
  char updatedAt[32];
  uint32_t sequence = 0;
  char sha256[65];  // 64 hex chars + NUL
  uint32_t bytes = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  char format[8];
  char contentType[16];
  char bmpUrl[192];
  int16_t sunriseLocalMinutes = -1;  // -1 = absent
  int16_t sunsetLocalMinutes = -1;   // -1 = absent

  // Required X4 geometry.
  static constexpr uint16_t REQUIRED_WIDTH = 480;
  static constexpr uint16_t REQUIRED_HEIGHT = 800;

  // Reset to zero/empty state.
  void clear();

  // Validate required fields. Returns true on success; on false, `message` is
  // filled with a short human-readable reason.
  bool validate(const char*& message) const;
};

// Parse a manifest JSON body into `out`. Allocation-free.
// Returns true on success. On false, `out` is left in cleared state and
// `errMessage` (if non-null) is set to a short static string reason.
bool parseManifest(const char* json, size_t jsonLen, RainmakerManifest& out, const char*& errMessage);

}  // namespace rainmaker
