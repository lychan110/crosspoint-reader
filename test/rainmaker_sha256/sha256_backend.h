#pragma once

#include "rainmaker/Sha256.h"

#include <cstddef>
#include <cstdint>

namespace rainmaker {
namespace backend {

// Compute SHA-256 of [data, data+len) into out[32].
// Returns true on success. Implementations are provided by exactly one of:
//   - sha256_mbedtls.h   (when HOST_SHA256_USE_MBEDTLS is defined)
//   - sha256_openssl.h   (when HOST_SHA256_USE_OPENSSL is defined)
//   - sha256_standalone.h (when HOST_SHA256_USE_STANDALONE is defined)
bool compute(const uint8_t* data, std::size_t len, uint8_t out[Sha256::DIGEST_BYTES]);

}  // namespace backend
}  // namespace rainmaker

#if defined(HOST_SHA256_USE_MBEDTLS)
#include "sha256_mbedtls.h"
#elif defined(HOST_SHA256_USE_OPENSSL)
#include "sha256_openssl.h"
#elif defined(HOST_SHA256_USE_STANDALONE)
#include "sha256_standalone.h"
#else
#error "No SHA-256 backend selected. Define HOST_SHA256_USE_MBEDTLS, HOST_SHA256_USE_OPENSSL, or HOST_SHA256_USE_STANDALONE."
#endif
