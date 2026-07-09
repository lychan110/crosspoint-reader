#pragma once

#include <GfxRenderer.h>

namespace rainmaker {

// Render the cached Rainmaker dashboard BMP as a full-screen sleep image.
// Falls back to a "no dashboard" placeholder if the cache is missing or the
// BMP can't be parsed. Intended to be called only from SleepActivity when
// the user's selected sleep screen mode is RAINMAKER.
void renderSleepScreen(GfxRenderer& renderer);

}  // namespace rainmaker
