#include "RainmakerManualSyncActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "RainmakerSyncService.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace rainmaker {

namespace {

const char* statusToStr(RainmakerSyncStatus s) {
  switch (s) {
    case RainmakerSyncStatus::Ok:
      return nullptr;  // success path uses STR_RAINMAKER_SYNC_OK_*
    case RainmakerSyncStatus::Disabled:
      return I18n::getInstance().get(StrId::STR_RAINMAKER_SYNC_DISABLED);
    case RainmakerSyncStatus::MissingConfig:
      return I18n::getInstance().get(StrId::STR_RAINMAKER_SYNC_MISSING_CONFIG);
    case RainmakerSyncStatus::LowBattery:
      return I18n::getInstance().get(StrId::STR_RAINMAKER_SYNC_LOW_BATTERY);
    case RainmakerSyncStatus::WifiFailed:
      return I18n::getInstance().get(StrId::STR_RAINMAKER_SYNC_WIFI_FAILED);
    case RainmakerSyncStatus::ManifestFetchFailed:
      return I18n::getInstance().get(StrId::STR_RAINMAKER_SYNC_MANIFEST_FAILED);
    case RainmakerSyncStatus::ManifestInvalid:
      return I18n::getInstance().get(StrId::STR_RAINMAKER_SYNC_MANIFEST_INVALID);
    case RainmakerSyncStatus::DownloadFailed:
      return I18n::getInstance().get(StrId::STR_RAINMAKER_SYNC_DOWNLOAD_FAILED);
    case RainmakerSyncStatus::HashMismatch:
      return I18n::getInstance().get(StrId::STR_RAINMAKER_SYNC_HASH_MISMATCH);
    case RainmakerSyncStatus::FileError:
      return I18n::getInstance().get(StrId::STR_RAINMAKER_SYNC_FILE_ERROR);
  }
  return nullptr;
}

}  // namespace

void RainmakerManualSyncActivity::onEnter() {
  Activity::onEnter();
  state = State::Idle;
  statusMessage[0] = '\0';
  runSync();
}

void RainmakerManualSyncActivity::onExit() { Activity::onExit(); }

void RainmakerManualSyncActivity::runSync() {
  {
    RenderLock lock(*this);
    state = State::Running;
  }
  requestUpdateAndWait();

  LOG_INF("RMK", "manual sync starting");
  const auto result = sync(RainmakerSyncMode::Manual);
  LOG_INF("RMK", "manual sync done: status=%d changed=%d msg=%s", static_cast<int>(result.status),
          result.changed ? 1 : 0, result.message);

  {
    RenderLock lock(*this);
    if (result.status == RainmakerSyncStatus::Ok) {
      state = result.changed ? State::DoneOk : State::DoneOkNoChange;
      statusMessage[0] = '\0';
    } else {
      state = State::DoneFailed;
      const char* base = statusToStr(result.status);
      if (!base || base[0] == '\0') base = result.message;
      snprintf(statusMessage, sizeof(statusMessage), "%s", base && base[0] ? base : "failed");
    }
  }
  requestUpdate();
}

void RainmakerManualSyncActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (state != State::Running) {
      goBack();
    }
  }
}

void RainmakerManualSyncActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_RAINMAKER_SYNC_NOW));

  const int centerY = pageHeight / 2;
  switch (state) {
    case State::Running:
      renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_RAINMAKER_CONNECTING), true);
      break;
    case State::DoneOk:
      renderer.drawCenteredText(UI_10_FONT_ID, centerY - 10, tr(STR_RAINMAKER_SYNC_OK_UPDATED), true,
                                EpdFontFamily::BOLD);
      break;
    case State::DoneOkNoChange:
      renderer.drawCenteredText(UI_10_FONT_ID, centerY - 10, tr(STR_RAINMAKER_SYNC_OK_CURRENT), true,
                                EpdFontFamily::BOLD);
      break;
    case State::DoneFailed:
      renderer.drawCenteredText(UI_10_FONT_ID, centerY - 10, tr(STR_ERROR_MSG), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, centerY + 15, statusMessage, true);
      break;
    case State::Idle:
    default:
      break;
  }

  if (state != State::Running) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}

}  // namespace rainmaker
