# sitecustomize.py — loaded automatically by Python at startup if present on
# sys.path. Monkey-patches certifi.where() to return our combined CA bundle
# so PlatformIO's HTTPSession (which ignores REQUESTS_CA_BUNDLE) trusts the
# Cloudflare MITM root that fronts github.com in sandboxed environments.
#
# Activated by `bin/pio` (or by setting PYTHONPATH to include `bin/`).
# No-op on hosts where the combined CA bundle is missing.

import os
import sys

try:
    import certifi  # noqa: E402
except ImportError:
    certifi = None

_CA = os.environ.get("PLATFORMIO_COMBINED_CA")


def _install() -> None:
    if certifi is None or not _CA or not os.path.isfile(_CA):
        return
    if getattr(certifi, "_rainmaker_patched", False):
        return
    certifi.where = lambda: _CA  # type: ignore[assignment]
    core = getattr(certifi, "core", None)
    if core is not None:
        core.where = lambda: _CA  # type: ignore[assignment]
    certifi._rainmaker_patched = True  # type: ignore[attr-defined]


_install()
