#pragma once

#include "sha256_backend.h"

#include <mbedtls/sha256.h>

namespace rainmaker {
namespace backend {

inline bool compute(const uint8_t* data, std::size_t len, uint8_t out[Sha256::DIGEST_BYTES]) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, data, len);
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
  return true;
}

}  // namespace backend
}  // namespace rainmaker
