#pragma once

#include "sha256_backend.h"

#include <openssl/evp.h>

#include <cstring>

namespace rainmaker {
namespace backend {

inline bool compute(const uint8_t* data, std::size_t len, uint8_t out[Sha256::DIGEST_BYTES]) {
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) return false;
  bool ok = false;
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
      EVP_DigestUpdate(ctx, data, len) == 1 &&
      EVP_DigestFinal_ex(ctx, out, nullptr) == 1) {
    ok = true;
  }
  EVP_MD_CTX_free(ctx);
  return ok;
}

}  // namespace backend
}  // namespace rainmaker
