#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "sha256_backend.h"

namespace rainmaker {
namespace host {

inline bool hostHashBuffer(const uint8_t* data, std::size_t len, char* outHex, std::size_t outHexSize) {
  if (outHexSize < Sha256::HEX_LEN + 1) return false;

  uint8_t digest[Sha256::DIGEST_BYTES];
  if (!backend::compute(data, len, digest)) return false;

  static const char hex[] = "0123456789abcdef";
  for (std::size_t i = 0; i < Sha256::DIGEST_BYTES; ++i) {
    outHex[i * 2] = hex[(digest[i] >> 4) & 0x0F];
    outHex[i * 2 + 1] = hex[digest[i] & 0x0F];
  }
  outHex[Sha256::HEX_LEN] = '\0';
  return true;
}

// Mirrors rainmaker::Sha256::equalsHex: case-insensitive compare of two
// 64-char hex strings; rejects nullptr or short inputs.
inline bool hostEqualsHex(const char* a, const char* b) {
  if (!a || !b) return false;
  for (std::size_t i = 0; i < Sha256::HEX_LEN; ++i) {
    const int ca = std::tolower(static_cast<unsigned char>(a[i]));
    const int cb = std::tolower(static_cast<unsigned char>(b[i]));
    if (ca != cb) return false;
    if (ca == '\0' || cb == '\0') return false;
  }
  return true;
}

}  // namespace host
}  // namespace rainmaker
