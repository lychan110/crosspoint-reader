#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "Sha256HostShim.h"

namespace {

using rainmaker::Sha256;
using rainmaker::host::hostEqualsHex;
using rainmaker::host::hostHashBuffer;

constexpr const char* SHA256_EMPTY = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

constexpr const char* SHA256_ABC = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

TEST(RainmakerSha256, EmptyString_KnownVector) {
  char hex[Sha256::HEX_LEN + 1] = {0};
  ASSERT_TRUE(hostHashBuffer(nullptr, 0, hex, sizeof(hex)));
  EXPECT_STREQ(hex, SHA256_EMPTY);
  EXPECT_EQ(std::strlen(hex), Sha256::HEX_LEN);
}

TEST(RainmakerSha256, HashBuffer_Abc_KnownVector) {
  const char* msg = "abc";
  char hex[Sha256::HEX_LEN + 1] = {0};
  ASSERT_TRUE(hostHashBuffer(reinterpret_cast<const uint8_t*>(msg), 3, hex, sizeof(hex)));
  EXPECT_STREQ(hex, SHA256_ABC);
}

TEST(RainmakerSha256, HashBuffer_LongerKnownVector) {
  // SHA-256 of the 56-byte ASCII string ("abcd..." repeated to 56 bytes, then
  // "abc") -- this is the FIPS 180-2 multi-block test vector input. The
  // expected digest verifies that block-boundary padding works correctly.
  const char* msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  char hex[Sha256::HEX_LEN + 1] = {0};
  ASSERT_TRUE(hostHashBuffer(reinterpret_cast<const uint8_t*>(msg), std::strlen(msg), hex, sizeof(hex)));
  EXPECT_STREQ(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(RainmakerSha256, HashBuffer_RejectsSmallOutput) {
  char hex[Sha256::HEX_LEN] = {0};  // one byte short
  EXPECT_FALSE(hostHashBuffer(nullptr, 0, hex, sizeof(hex)));
}

TEST(RainmakerSha256, EqualsHex_CaseInsensitive) {
  // Lowercase vs uppercase.
  EXPECT_TRUE(hostEqualsHex(SHA256_ABC,
                            "BA7816BF8F01CFEA414140DE5DAE2223"
                            "B00361A396177A9CB410FF61F20015AD"));
  // Mixed case.
  EXPECT_TRUE(hostEqualsHex(SHA256_ABC,
                            "bA7816bF8f01cFea414140De5dAe2223"
                            "B00361A396177A9CB410fF61F20015ad"));
  // Both lowercase.
  EXPECT_TRUE(hostEqualsHex(SHA256_ABC, SHA256_ABC));
}

TEST(RainmakerSha256, EqualsHex_Mismatch) {
  // One character off.
  char almost[Sha256::HEX_LEN + 1] = {0};
  std::memcpy(almost, SHA256_ABC, Sha256::HEX_LEN);
  almost[10] = (almost[10] == 'a') ? 'b' : 'a';
  EXPECT_FALSE(hostEqualsHex(SHA256_ABC, almost));
}

TEST(RainmakerSha256, EqualsHex_NullArguments) {
  EXPECT_FALSE(hostEqualsHex(nullptr, SHA256_ABC));
  EXPECT_FALSE(hostEqualsHex(SHA256_ABC, nullptr));
  EXPECT_FALSE(hostEqualsHex(nullptr, nullptr));
}

TEST(RainmakerSha256, EqualsHex_ShortRejection) {
  // A 5-char string is shorter than 64. The first NUL encountered inside the
  // 64-iteration compare must trigger a false return.
  const char* shortA = "ba781";  // matches the first 5 chars of SHA256_ABC
  const char* shortB = "ba781";
  EXPECT_FALSE(hostEqualsHex(shortA, shortB));

  // A short prefix of a 64-char string still fails because the loop reads
  // past the NUL and triggers the early-exit.
  EXPECT_FALSE(hostEqualsHex(shortA, SHA256_ABC));
  EXPECT_FALSE(hostEqualsHex(SHA256_ABC, shortA));

  // Two identical prefixes that are the same length as each other also fail.
  const char* shortA2 = "abcdef";
  const char* shortB2 = "abcdef";
  EXPECT_FALSE(hostEqualsHex(shortA2, shortB2));
}

}  // namespace
