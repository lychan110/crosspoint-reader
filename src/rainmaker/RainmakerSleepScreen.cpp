#include "RainmakerSleepScreen.h"

#include <Bitmap.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include "RainmakerSyncState.h"
#include "fontIds.h"

namespace rainmaker {

namespace {
// cppcheck-suppress constParameterReference
void renderMissingDashboard(GfxRenderer& renderer) {
  const auto pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_RAINMAKER_NO_DASHBOARD), true, EpdFontFamily::BOLD);
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}
}  // namespace

void renderSleepScreen(GfxRenderer& renderer) {
  if (!hasCachedDashboard()) {
    LOG_DBG("RMK", "no cached dashboard; falling back to placeholder");
    return renderMissingDashboard(renderer);
  }

  HalFile file;
  if (!Storage.openFileForRead("RMK", RainmakerSyncState::CACHE_BMP, file)) {
    LOG_WRN("RMK", "failed to open cached dashboard");
    return renderMissingDashboard(renderer);
  }

  // Reuse the same parser as SleepActivity. The cached BMP is 480x800
  // 1-bit, but the parser handles grayscale 24-bit too if Rainmaker ever
  // produces that format.
  Bitmap bitmap(file, true);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) {
    LOG_WRN("RMK", "cached dashboard header parse failed");
    file.close();
    return renderMissingDashboard(renderer);
  }

  renderer.clearScreen();
  renderer.drawBitmap(bitmap, 0, 0, renderer.getScreenWidth(), renderer.getScreenHeight(), 0.0f, 0.0f);

  if (bitmap.hasGreyscale()) {
    renderer.displayGrayscaleBase(HalDisplay::FULL_REFRESH);
  } else {
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  }
  if (bitmap.hasGreyscale()) {
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, 0, 0, renderer.getScreenWidth(), renderer.getScreenHeight(), 0.0f, 0.0f);
    renderer.copyGrayscaleLsbBuffers();
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, 0, 0, renderer.getScreenWidth(), renderer.getScreenHeight(), 0.0f, 0.0f);
    renderer.copyGrayscaleMsbBuffers();
    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  }

  file.close();
}

}  // namespace rainmaker
