#include "RainmakerSyncState.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace rainmaker {

void RainmakerSyncState::clear() {
  lastSha256[0] = '\0';
  sequence = 0;
  updatedAt[0] = '\0';
  sunriseLocalMinutes = -1;
  sunsetLocalMinutes = -1;
  lastAttemptEpoch = 0;
  lastSuccessEpoch = 0;
  lastError[0] = '\0';
}

namespace {

void copyBounded(char* dest, size_t destSize, const char* src) {
  if (destSize == 0) return;
  size_t i = 0;
  if (src) {
    for (; i < destSize - 1 && src[i] != '\0'; ++i) dest[i] = src[i];
  }
  dest[i] = '\0';
}

}  // namespace

bool loadState(RainmakerSyncState& state) {
  state.clear();
  if (!Storage.exists(RainmakerSyncState::STATE_FILE)) {
    return false;
  }
  const String json = Storage.readFile(RainmakerSyncState::STATE_FILE);
  if (json.isEmpty()) return false;

  JsonDocument doc;
  const auto err = deserializeJson(doc, json.c_str(), json.length());
  if (err) {
    LOG_INF("RMK", "state json parse failed: %s", err.c_str());
    return false;
  }

  copyBounded(state.lastSha256, sizeof(state.lastSha256), doc["lastSha256"] | "");
  state.sequence = doc["sequence"] | static_cast<uint32_t>(0);
  copyBounded(state.updatedAt, sizeof(state.updatedAt), doc["updatedAt"] | "");

  if (doc["sunriseLocalMinutes"].is<int>()) {
    const int v = doc["sunriseLocalMinutes"].as<int>();
    state.sunriseLocalMinutes = (v >= 0 && v <= 1439) ? static_cast<int16_t>(v) : -1;
  }
  if (doc["sunsetLocalMinutes"].is<int>()) {
    const int v = doc["sunsetLocalMinutes"].as<int>();
    state.sunsetLocalMinutes = (v >= 0 && v <= 1439) ? static_cast<int16_t>(v) : -1;
  }
  state.lastAttemptEpoch = doc["lastAttemptEpoch"] | static_cast<uint32_t>(0);
  state.lastSuccessEpoch = doc["lastSuccessEpoch"] | static_cast<uint32_t>(0);
  copyBounded(state.lastError, sizeof(state.lastError), doc["lastError"] | "");
  return true;
}

bool saveState(const RainmakerSyncState& state) {
  Storage.mkdir(RainmakerSyncState::STATE_DIR);

  JsonDocument doc;
  doc["lastSha256"] = state.lastSha256;
  doc["sequence"] = state.sequence;
  doc["updatedAt"] = state.updatedAt;
  if (state.sunriseLocalMinutes >= 0) doc["sunriseLocalMinutes"] = state.sunriseLocalMinutes;
  if (state.sunsetLocalMinutes >= 0) doc["sunsetLocalMinutes"] = state.sunsetLocalMinutes;
  doc["lastAttemptEpoch"] = state.lastAttemptEpoch;
  doc["lastSuccessEpoch"] = state.lastSuccessEpoch;
  doc["lastError"] = state.lastError;

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(RainmakerSyncState::STATE_FILE, json);
}

bool hasCachedDashboard() { return Storage.exists(RainmakerSyncState::CACHE_BMP); }

const char* cachedDashboardPath() { return RainmakerSyncState::CACHE_BMP; }

}  // namespace rainmaker
