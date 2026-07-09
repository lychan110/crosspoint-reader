#!/usr/bin/env bash
# Install CrossPoint / Crosspoint-Rainmaker build & dev dependencies on Ubuntu 22.04.
# Run from the repo root. Idempotent.
#
# Covers what the upstream README + the fork's CONTRIBUTING/CONTEXT demand:
#   - PlatformIO Core CLI (the "pioarduino" is just a platform= URL — stock pio works)
#   - clang-format >= 21 (apt only has 14 on jammy)
#   - Python tooling for gen_i18n.py, debugging_monitor.py, and the build scripts
#   - freeink-sdk git submodule
#   - Project-local PlatformIO state (default ~/.platformio is sandbox-locked here)

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-${REPO_ROOT}/.pio-platformio}"
CACERT_SRC="/etc/cloudflare/certs/cloudflare-containers-ca.crt"
CACERT_DST="${PLATFORMIO_CORE_DIR}/combined-ca.crt"
SHARED_CACERT_SRC="/etc/ssl/certs/ca-certificates.crt"

# 1. System packages (apt).
#    python3-pip       -> platformio, clang-format pip wheel
#    python3.10-venv  -> required by PlatformIO's penv on jammy
#    git              -> already present, included for completeness
#    g++              -> host test builds (cmake + GoogleTest)
#    cmake            -> host test builds
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    python3-pip python3.10-venv git ca-certificates \
    g++ cmake

# 2. Python packages via pip.
#    PlatformIO Core CLI
#    clang-format 21   (apt only ships 14; .clang-format requires >= 21)
#    pyserial, colorama, matplotlib, pyyaml — debugging_monitor.py + gen_i18n.py
python3 -m pip install --upgrade pip
python3 -m pip install \
    platformio \
    'clang-format>=21,<22' \
    pyserial colorama matplotlib pyyaml

# 3. Combine system CA bundle with the cloudflare-container CA.
#    In sandboxed/cloudflared environments, github.com is served by an MITM
#    proxy whose chain ends in a self-signed cloudflare root. PlatformIO's
#    HTTPSession ignores REQUESTS_CA_BUNDLE; only the cert that certifi bundles
#    (or the one curl/python ssl trust) sees the right chain.
mkdir -p "${PLATFORMIO_CORE_DIR}"
if [[ -r "${CACERT_SRC}" && -r "${SHARED_CACERT_SRC}" ]]; then
    cat "${SHARED_CACERT_SRC}" "${CACERT_SRC}" > "${CACERT_DST}"
    echo "wrote ${CACERT_DST}"
else
    echo "warning: could not build combined CA bundle (${CACERT_SRC} or ${SHARED_CACERT_SRC} unreadable)" >&2
fi

# 4. Init the freeink-sdk submodule if needed.
if [[ ! -d "${REPO_ROOT}/freeink-sdk/libs" ]]; then
    git -C "${REPO_ROOT}" submodule update --init --recursive
fi

# 5. Sanity checks.
clang-format --version | head -1
pio --version
python3 scripts/gen_i18n.py >/dev/null && echo "gen_i18n.py: ok"

cat <<EOF

Next steps:
  export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR}"
  pio run -e default    # build the default env
  ./bin/clang-format-fix
