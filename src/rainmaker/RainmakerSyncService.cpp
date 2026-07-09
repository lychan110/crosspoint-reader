#include "RainmakerSyncService.h"

#include <HalPowerManager.h>
#include <HalStorage.h>
#include <HttpDownloader.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "RainmakerManifest.h"
#include "RainmakerSyncState.h"
#include "Sha256.h"
#include "WifiCredentialStore.h"

namespace rainmaker {

namespace {

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr int WIFI_STATUS_POLL_MS = 100;

void setResult(RainmakerSyncResult& r, RainmakerSyncStatus s, const char* msg) {
  r.status = s;
  r.changed = false;
  if (msg) {
    strncpy(r.message, msg, sizeof(r.message) - 1);
    r.message[sizeof(r.message) - 1] = '\0';
  } else {
    r.message[0] = '\0';
  }
}

void setResultOk(RainmakerSyncResult& r, bool changed, uint32_t bytes, const char* msg) {
  r.status = RainmakerSyncStatus::Ok;
  r.changed = changed;
  r.bytesDownloaded = bytes;
  if (msg) {
    strncpy(r.message, msg, sizeof(r.message) - 1);
    r.message[sizeof(r.message) - 1] = '\0';
  } else {
    r.message[0] = '\0';
  }
}

// Read settings (URL/credentials/battery threshold) and populate a result on
// failure. Returns true if config is valid; false on any issue.
bool validateConfig(RainmakerSyncResult& r) {
  if (!SETTINGS.rainmakerSyncEnabled) {
    setResult(r, RainmakerSyncStatus::Disabled, "sync disabled");
    return false;
  }
  if (SETTINGS.rainmakerManifestUrl[0] == '\0') {
    setResult(r, RainmakerSyncStatus::MissingConfig, "manifest URL not set");
    return false;
  }
  if (SETTINGS.rainmakerUsername[0] == '\0' || SETTINGS.rainmakerPassword[0] == '\0') {
    setResult(r, RainmakerSyncStatus::MissingConfig, "credentials not set");
    return false;
  }
  return true;
}

// Try to connect to the last-saved WiFi network. Returns true on success.
// Mirrors WifiSelectionActivity::attemptConnection minus the UI. Caller is
// responsible for leaving the modem in a usable state on success and
// tearing it down on failure.
bool connectToSavedWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(50);

  String lastSsid;
  if (!WIFI_STORE.getLastConnectedSsid().empty()) {
    lastSsid = WIFI_STORE.getLastConnectedSsid().c_str();
  }
  if (lastSsid.isEmpty()) return false;

  const auto* cred = WIFI_STORE.findCredential(lastSsid.c_str());
  if (!cred) return false;

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String hostname = "CrossPoint-Reader-" + mac;
  WiFi.setHostname(hostname.c_str());

  if (!cred->password.empty()) {
    WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
  } else {
    WiFi.begin(cred->ssid.c_str());
  }

  const unsigned long start = millis();
  while (millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(WIFI_STATUS_POLL_MS);
  }
  return false;
}

void teardownWifi() {
  if (WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
}

// Build a std::string copy of the user's manifest URL / username / password,
// NUL-truncating at the buffer boundary.
std::string settingsString(const char* s) { return std::string(s); }

}  // namespace

RainmakerSyncResult sync(RainmakerSyncMode mode) {
  RainmakerSyncResult r;
  setResultOk(r, false, 0, "");

  if (!validateConfig(r)) {
    return r;
  }

  // Battery guard. Manual mode skips this and lets the user decide.
  if (mode == RainmakerSyncMode::Scheduled) {
    const uint16_t pct = powerManager.getBatteryPercentage();
    if (pct < SETTINGS.rainmakerMinBatteryPercent) {
      char msg[64];
      snprintf(msg, sizeof(msg), "battery %u%% < %u%%", pct, SETTINGS.rainmakerMinBatteryPercent);
      setResult(r, RainmakerSyncStatus::LowBattery, msg);
      return r;
    }
  }

  if (!connectToSavedWifi()) {
    setResult(r, RainmakerSyncStatus::WifiFailed, "wifi connect failed");
    return r;
  }

  // Fetch the manifest.
  const std::string url = settingsString(SETTINGS.rainmakerManifestUrl);
  const std::string user = settingsString(SETTINGS.rainmakerUsername);
  const std::string pass = settingsString(SETTINGS.rainmakerPassword);

  std::string body;
  if (!HttpDownloader::fetchUrl(url, body, user, pass)) {
    setResult(r, RainmakerSyncStatus::ManifestFetchFailed, "manifest fetch failed");
    teardownWifi();
    return r;
  }

  RainmakerManifest manifest;
  const char* errMsg = nullptr;
  if (!parseManifest(body.c_str(), body.size(), manifest, errMsg)) {
    char msg[64];
    snprintf(msg, sizeof(msg), "manifest: %s", errMsg ? errMsg : "invalid");
    setResult(r, RainmakerSyncStatus::ManifestInvalid, msg);
    teardownWifi();
    return r;
  }

  // Update runtime state with new solar times and the last attempt time.
  RainmakerSyncState state;
  loadState(state);
  state.lastAttemptEpoch = static_cast<uint32_t>(time(nullptr));
  if (manifest.sunriseLocalMinutes >= 0) state.sunriseLocalMinutes = manifest.sunriseLocalMinutes;
  if (manifest.sunsetLocalMinutes >= 0) state.sunsetLocalMinutes = manifest.sunsetLocalMinutes;

  // Already current? Save the new attempt timestamp and return early.
  if (Sha256::equalsHex(state.lastSha256, manifest.sha256) && hasCachedDashboard()) {
    state.lastError[0] = '\0';
    saveState(state);
    teardownWifi();
    setResultOk(r, false, 0, "already current");
    return r;
  }

  // Ensure cache dir exists; clean up any leftover temp.
  Storage.mkdir(RainmakerSyncState::STATE_DIR);
  if (Storage.exists(RainmakerSyncState::CACHE_TMP)) {
    Storage.remove(RainmakerSyncState::CACHE_TMP);
  }

  // Download to a temp file.
  const std::string bmpUrl = settingsString(manifest.bmpUrl);
  const auto err = HttpDownloader::downloadToFile(bmpUrl, RainmakerSyncState::CACHE_TMP, nullptr, nullptr, user, pass);
  if (err != HttpDownloader::OK) {
    setResult(r, RainmakerSyncStatus::DownloadFailed, "bmp download failed");
    Storage.remove(RainmakerSyncState::CACHE_TMP);
    saveState(state);
    teardownWifi();
    return r;
  }

  // Verify byte count.
  HalFile f;
  if (!Storage.openFileForRead("RMK", RainmakerSyncState::CACHE_TMP, f)) {
    setResult(r, RainmakerSyncStatus::FileError, "open temp failed");
    Storage.remove(RainmakerSyncState::CACHE_TMP);
    saveState(state);
    teardownWifi();
    return r;
  }
  const size_t actualBytes = f.size();
  f.close();
  if (actualBytes != manifest.bytes) {
    char msg[80];
    snprintf(msg, sizeof(msg), "size mismatch: %u != %u", static_cast<unsigned>(actualBytes),
             static_cast<unsigned>(manifest.bytes));
    setResult(r, RainmakerSyncStatus::HashMismatch, msg);
    Storage.remove(RainmakerSyncState::CACHE_TMP);
    saveState(state);
    teardownWifi();
    return r;
  }

  // Verify SHA-256.
  char hexOut[Sha256::HEX_LEN + 1] = {};
  if (!Sha256::hashFile(RainmakerSyncState::CACHE_TMP, hexOut, sizeof(hexOut))) {
    setResult(r, RainmakerSyncStatus::FileError, "sha256 read failed");
    Storage.remove(RainmakerSyncState::CACHE_TMP);
    saveState(state);
    teardownWifi();
    return r;
  }
  if (!Sha256::equalsHex(hexOut, manifest.sha256)) {
    setResult(r, RainmakerSyncStatus::HashMismatch, "sha256 mismatch");
    Storage.remove(RainmakerSyncState::CACHE_TMP);
    saveState(state);
    teardownWifi();
    return r;
  }

  // Atomic replace: remove old cache, rename tmp -> cache.
  if (Storage.exists(RainmakerSyncState::CACHE_BMP)) {
    Storage.remove(RainmakerSyncState::CACHE_BMP);
  }
  if (!Storage.rename(RainmakerSyncState::CACHE_TMP, RainmakerSyncState::CACHE_BMP)) {
    setResult(r, RainmakerSyncStatus::FileError, "rename failed");
    Storage.remove(RainmakerSyncState::CACHE_TMP);
    saveState(state);
    teardownWifi();
    return r;
  }

  // Commit successful state.
  strncpy(state.lastSha256, manifest.sha256, sizeof(state.lastSha256) - 1);
  state.lastSha256[sizeof(state.lastSha256) - 1] = '\0';
  state.sequence = manifest.sequence;
  strncpy(state.updatedAt, manifest.updatedAt, sizeof(state.updatedAt) - 1);
  state.updatedAt[sizeof(state.updatedAt) - 1] = '\0';
  state.lastSuccessEpoch = static_cast<uint32_t>(time(nullptr));
  state.lastError[0] = '\0';
  saveState(state);
  teardownWifi();

  setResultOk(r, true, static_cast<uint32_t>(actualBytes), "updated");
  return r;
}

}  // namespace rainmaker
