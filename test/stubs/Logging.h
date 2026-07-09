// Host stub for Logging.h. The on-device header is a printf wrapper. The
// production modules we link from host tests do not require any symbols from
// it at link time, so an empty header is sufficient.
#pragma once

#include <cstdio>
#include <cstdarg>

// Minimal no-op shim that matches the macro surface the production code
// expects. The host tests do not assert on log output, so all macros are
// inert. Defining them as empty also avoids relying on a particular Logging
// revision.
#define LOG_ERR(tag, fmt, ...) ((void)0)
#define LOG_WRN(tag, fmt, ...) ((void)0)
#define LOG_INF(tag, fmt, ...) ((void)0)
#define LOG_DBG(tag, fmt, ...) ((void)0)
