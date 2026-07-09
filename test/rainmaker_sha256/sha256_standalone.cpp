// Standalone FIPS 180-4 SHA-256 implementation, used as a fallback when neither
// mbedtls nor OpenSSL is available on the host. Adapted from the public-domain
// reference; see https://csrc.nist.gov/publications/detail/fips/180/4/final
// for the algorithm specification.

#include "sha256_standalone.h"

#include <cstring>

namespace rainmaker {
namespace backend {

namespace {

constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

void init(StandaloneCtx& ctx) {
  ctx.state[0] = 0x6a09e667;
  ctx.state[1] = 0xbb67ae85;
  ctx.state[2] = 0x3c6ef372;
  ctx.state[3] = 0xa54ff53a;
  ctx.state[4] = 0x510e527f;
  ctx.state[5] = 0x9b05688c;
  ctx.state[6] = 0x1f83d9ab;
  ctx.state[7] = 0x5be0cd19;
  ctx.bufferLen = 0;
  ctx.totalBits = 0;
}

void transform(StandaloneCtx& ctx, const uint8_t block[64]) {
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2], d = ctx.state[3];
  uint32_t e = ctx.state[4], f = ctx.state[5], g = ctx.state[6], h = ctx.state[7];
  for (int i = 0; i < 64; ++i) {
    const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const uint32_t ch = (e & f) ^ (~e & g);
    const uint32_t t1 = h + S1 + ch + K[i] + w[i];
    const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = S0 + mj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  ctx.state[0] += a;
  ctx.state[1] += b;
  ctx.state[2] += c;
  ctx.state[3] += d;
  ctx.state[4] += e;
  ctx.state[5] += f;
  ctx.state[6] += g;
  ctx.state[7] += h;
}

void update(StandaloneCtx& ctx, const uint8_t* data, std::size_t len) {
  ctx.totalBits += static_cast<uint64_t>(len) * 8;
  while (len > 0) {
    std::size_t take = 64 - ctx.bufferLen;
    if (take > len) take = len;
    std::memcpy(ctx.buffer + ctx.bufferLen, data, take);
    ctx.bufferLen += static_cast<uint32_t>(take);
    data += take;
    len -= take;
    if (ctx.bufferLen == 64) {
      transform(ctx, ctx.buffer);
      ctx.bufferLen = 0;
    }
  }
}

void finish(StandaloneCtx& ctx, uint8_t out[32]) {
  const uint64_t bits = ctx.totalBits;
  const uint8_t one = 0x80;
  update(ctx, &one, 1);
  const uint8_t zero = 0;
  while (ctx.bufferLen != 56) {
    update(ctx, &zero, 1);
  }
  uint8_t lenBytes[8];
  for (int i = 0; i < 8; ++i) {
    lenBytes[i] = static_cast<uint8_t>((bits >> (56 - i * 8)) & 0xFF);
  }
  update(ctx, lenBytes, 8);
  for (int i = 0; i < 8; ++i) {
    out[i * 4] = static_cast<uint8_t>((ctx.state[i] >> 24) & 0xFF);
    out[i * 4 + 1] = static_cast<uint8_t>((ctx.state[i] >> 16) & 0xFF);
    out[i * 4 + 2] = static_cast<uint8_t>((ctx.state[i] >> 8) & 0xFF);
    out[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i] & 0xFF);
  }
}

}  // namespace

bool compute(const uint8_t* data, std::size_t len, uint8_t out[32]) {
  StandaloneCtx ctx;
  init(ctx);
  update(ctx, data, len);
  finish(ctx, out);
  return true;
}

}  // namespace backend
}  // namespace rainmaker
