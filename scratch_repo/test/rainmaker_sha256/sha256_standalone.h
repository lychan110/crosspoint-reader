#pragma once

#include <cstddef>
#include <cstdint>

#include "rainmaker/Sha256.h"

namespace rainmaker {
namespace backend {

// FIPS 180-4 SHA-256 context, used by the standalone fallback backend.
struct StandaloneCtx {
  uint32_t state[8];
  uint8_t buffer[64];
  uint32_t bufferLen;
  uint64_t totalBits;
};

}  // namespace backend
}  // namespace rainmaker
