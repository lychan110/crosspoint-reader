# Tries to use the system mbedtls first, then OpenSSL's libcrypto, and falls
# back to a small embedded public-domain SHA-256 implementation if neither is
# found. The host test does not link the production Sha256.cpp (it pulls in
# HalStorage); instead it calls the backend through Sha256HostShim.h, which
# exposes the same hashBuffer/equalsHex surface the production code uses.
add_executable(RainmakerSha256Test
  RainmakerSha256Test.cpp
)

target_include_directories(RainmakerSha256Test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REPO_ROOT}/src
)

find_package(MbedTLS QUIET)
if(MbedTLS_FOUND)
  message(STATUS "RainmakerSha256Test: using mbedtls backend")
  target_compile_definitions(RainmakerSha256Test PRIVATE HOST_SHA256_USE_MBEDTLS=1)
  target_link_libraries(RainmakerSha256Test PRIVATE MbedTLS::mbedcrypto)
else()
  find_package(OpenSSL QUIET)
  if(OpenSSL_FOUND)
    message(STATUS "RainmakerSha256Test: using OpenSSL EVP_Digest backend")
    target_compile_definitions(RainmakerSha256Test PRIVATE HOST_SHA256_USE_OPENSSL=1)
    target_link_libraries(RainmakerSha256Test PRIVATE OpenSSL::Crypto)
  else()
    message(STATUS
      "RainmakerSha256Test: mbedtls and OpenSSL not found; using embedded "
      "standalone SHA-256 backend (FIPS 180-4 reference implementation)")
    target_sources(RainmakerSha256Test PRIVATE sha256_standalone.cpp)
    target_compile_definitions(RainmakerSha256Test PRIVATE HOST_SHA256_USE_STANDALONE=1)
  endif()
endif()

target_link_libraries(RainmakerSha256Test PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(RainmakerSha256Test)
