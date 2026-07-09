#pragma once

#include <cstddef>
#include <cstdint>

namespace rainmaker {

// Streaming SHA-256 over a file path on the SD card.
// Lowercase hex output; comparison against a stored lowercase hex digest.
class Sha256 {
 public:
  static constexpr size_t DIGEST_BYTES = 32;
  static constexpr size_t HEX_LEN = 64;

  // Compute SHA-256 of a file, writing 64-char lowercase hex (NUL-terminated) into outHex.
  // Returns true on success; false on file open error, read error, or insufficient buffer.
  // outHex must be at least HEX_LEN + 1 bytes.
  static bool hashFile(const char* path, char* outHex, size_t outHexSize);

  // Compute SHA-256 of a buffer in one shot.
  static bool hashBuffer(const uint8_t* data, size_t len, char* outHex, size_t outHexSize);

  // Compare two lowercase/uppercase hex strings case-insensitively.
  // Both must be at least 64 hex chars.
  static bool equalsHex(const char* a, const char* b);
};

}  // namespace rainmaker
