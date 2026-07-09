#include "Sha256.h"

#include <HalStorage.h>
#include <Logging.h>
#include <mbedtls/sha256.h>

#include <cctype>
#include <cstring>

namespace rainmaker {

namespace {

constexpr size_t READ_CHUNK = 2048;

inline void toHex(const uint8_t* bytes, size_t len, char* out) {
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < len; ++i) {
    out[i * 2] = hex[(bytes[i] >> 4) & 0x0F];
    out[i * 2 + 1] = hex[bytes[i] & 0x0F];
  }
  out[len * 2] = '\0';
}

}  // namespace

bool Sha256::hashFile(const char* path, char* outHex, size_t outHexSize) {
  if (outHexSize < HEX_LEN + 1) return false;
  HalFile file;
  if (!Storage.openFileForRead("RMK", path, file)) {
    LOG_ERR("RMK", "sha256: cannot open %s", path);
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);

  uint8_t buf[READ_CHUNK];
  while (true) {
    const int n = file.read(buf, READ_CHUNK);
    if (n < 0) {
      LOG_ERR("RMK", "sha256: read error on %s", path);
      mbedtls_sha256_free(&ctx);
      file.close();
      return false;
    }
    if (n == 0) break;
    mbedtls_sha256_update(&ctx, buf, static_cast<size_t>(n));
  }

  uint8_t digest[DIGEST_BYTES];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  file.close();

  toHex(digest, DIGEST_BYTES, outHex);
  return true;
}

bool Sha256::hashBuffer(const uint8_t* data, size_t len, char* outHex, size_t outHexSize) {
  if (outHexSize < HEX_LEN + 1) return false;
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, data, len);
  uint8_t digest[DIGEST_BYTES];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  toHex(digest, DIGEST_BYTES, outHex);
  return true;
}

bool Sha256::equalsHex(const char* a, const char* b) {
  if (!a || !b) return false;
  for (size_t i = 0; i < HEX_LEN; ++i) {
    const int ca = std::tolower(static_cast<unsigned char>(a[i]));
    const int cb = std::tolower(static_cast<unsigned char>(b[i]));
    if (ca != cb) return false;
    if (ca == '\0' || cb == '\0') return false;
  }
  return true;
}

}  // namespace rainmaker
