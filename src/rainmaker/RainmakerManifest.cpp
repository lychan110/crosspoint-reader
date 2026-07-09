#include "RainmakerManifest.h"

#include <ArduinoJson.h>
#include <Logging.h>

#include <cctype>
#include <cstring>

namespace rainmaker {

void RainmakerManifest::clear() {
  version = 0;
  id[0] = '\0';
  updatedAt[0] = '\0';
  sequence = 0;
  sha256[0] = '\0';
  bytes = 0;
  width = 0;
  height = 0;
  format[0] = '\0';
  contentType[0] = '\0';
  bmpUrl[0] = '\0';
  sunriseLocalMinutes = -1;
  sunsetLocalMinutes = -1;
}

namespace {

// Copy `src` into a fixed-size destination, NUL-terminating. Truncates safely
// if the source is longer than the destination. Returns true if any truncation
// happened (caller may use this to flag invalid input).
bool copyBounded(char* dest, size_t destSize, const char* src) {
  if (destSize == 0) return true;
  size_t i = 0;
  if (src) {
    for (; i < destSize - 1 && src[i] != '\0'; ++i) dest[i] = src[i];
  }
  dest[i] = '\0';
  return src != nullptr && i < std::strlen(src);
}

bool isHexString(const char* s, size_t expectedLen) {
  if (!s) return false;
  size_t i = 0;
  for (; s[i] != '\0' && i < expectedLen; ++i) {
    if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
  }
  return i == expectedLen;
}

constexpr const char* ERR_MISSING = "missing field";
constexpr const char* ERR_TYPE = "wrong field type";
constexpr const char* ERR_VERSION = "unsupported version";
constexpr const char* ERR_GEOMETRY = "image must be 480x800";
constexpr const char* ERR_SHA = "invalid sha256";
constexpr const char* ERR_OOM = "JSON doc allocation failed";
constexpr const char* ERR_TOO_LONG = "field too long";

}  // namespace

bool RainmakerManifest::validate(const char*& message) const {
  if (version != 1) {
    message = ERR_VERSION;
    return false;
  }
  if (sha256[0] == '\0' || !isHexString(sha256, 64)) {
    message = ERR_SHA;
    return false;
  }
  if (bytes == 0) {
    message = ERR_MISSING;
    return false;
  }
  if (width != REQUIRED_WIDTH || height != REQUIRED_HEIGHT) {
    message = ERR_GEOMETRY;
    return false;
  }
  if (bmpUrl[0] == '\0') {
    message = ERR_MISSING;
    return false;
  }
  return true;
}

bool parseManifest(const char* json, size_t jsonLen, RainmakerManifest& out, const char*& errMessage) {
  out.clear();
  errMessage = ERR_MISSING;

  if (!json || jsonLen == 0) {
    errMessage = ERR_MISSING;
    return false;
  }

  // The manifest is small (under 1 KB). JsonDocument uses inline storage.
  JsonDocument doc;
  const auto err = deserializeJson(doc, json, jsonLen);
  if (err == DeserializationError::NoMemory) {
    errMessage = ERR_OOM;
    return false;
  }
  if (err) {
    LOG_ERR("RMK", "manifest json parse: %s", err.c_str());
    errMessage = ERR_TYPE;
    return false;
  }

  // version
  if (doc["version"].is<uint8_t>()) {
    out.version = doc["version"].as<uint8_t>();
  } else if (doc["version"].is<int>()) {
    const int v = doc["version"].as<int>();
    if (v < 0 || v > 255) {
      errMessage = ERR_VERSION;
      return false;
    }
    out.version = static_cast<uint8_t>(v);
  } else {
    errMessage = ERR_MISSING;
    return false;
  }

  // Required strings: id, updatedAt, sha256, bmpUrl, format, contentType.
  // sha256/bmpUrl must not be truncated — they verify bytes or locate the file.
  copyBounded(out.id, sizeof(out.id), doc["id"] | "");
  copyBounded(out.updatedAt, sizeof(out.updatedAt), doc["updatedAt"] | "");
  if (copyBounded(out.sha256, sizeof(out.sha256), doc["sha256"] | "")) {
    errMessage = ERR_TOO_LONG;
    return false;
  }
  copyBounded(out.format, sizeof(out.format), doc["format"] | "");
  copyBounded(out.contentType, sizeof(out.contentType), doc["contentType"] | "");
  if (copyBounded(out.bmpUrl, sizeof(out.bmpUrl), doc["bmpUrl"] | "")) {
    errMessage = ERR_TOO_LONG;
    return false;
  }

  // Numbers
  if (doc["sequence"].is<uint32_t>()) {
    out.sequence = doc["sequence"].as<uint32_t>();
  } else if (doc["sequence"].is<int>()) {
    long long v = doc["sequence"].as<long long>();
    if (v < 0) v = 0;
    out.sequence = static_cast<uint32_t>(v);
  }
  if (doc["bytes"].is<uint32_t>()) {
    out.bytes = doc["bytes"].as<uint32_t>();
  } else if (doc["bytes"].is<int>()) {
    long long v = doc["bytes"].as<long long>();
    if (v < 0) v = 0;
    out.bytes = static_cast<uint32_t>(v);
  }
  if (doc["width"].is<uint16_t>()) {
    out.width = doc["width"].as<uint16_t>();
  } else if (doc["width"].is<int>()) {
    const int v = doc["width"].as<int>();
    out.width = v > 0 && v <= 65535 ? static_cast<uint16_t>(v) : 0;
  }
  if (doc["height"].is<uint16_t>()) {
    out.height = doc["height"].as<uint16_t>();
  } else if (doc["height"].is<int>()) {
    const int v = doc["height"].as<int>();
    out.height = v > 0 && v <= 65535 ? static_cast<uint16_t>(v) : 0;
  }

  // Optional solar minutes. -1 sentinel means "absent".
  if (doc["sunriseLocalMinutes"].is<int>()) {
    const int v = doc["sunriseLocalMinutes"].as<int>();
    if (v >= 0 && v <= 1439) {
      out.sunriseLocalMinutes = static_cast<int16_t>(v);
    }
  }
  if (doc["sunsetLocalMinutes"].is<int>()) {
    const int v = doc["sunsetLocalMinutes"].as<int>();
    if (v >= 0 && v <= 1439) {
      out.sunsetLocalMinutes = static_cast<int16_t>(v);
    }
  }

  if (!out.validate(errMessage)) {
    return false;
  }
  return true;
}

}  // namespace rainmaker
