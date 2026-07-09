# CI & Cloud-Agent Sandbox — Build/Test Reproducibility

This document is for **cloud agents and CI maintainers**, not end users.
It records the exact gotchas, fixes, and recipes needed to reproduce
GitHub Actions' `ci.yml` matrix in a cloud-agent sandbox. If you are
flashing the firmware to a real device, you want
[`build-and-flash.md`](build-and-flash.md) instead.

---

## TL;DR — the minimum set of commands

```bash
# 1. Install system + Python dependencies (one-shot, idempotent).
./bin/install-deps.sh

# 2. Init the freeink-sdk submodule (the script does it if missing).
git submodule update --init --recursive

# 3. Run the same checks GH Actions does — use bin/pio, not pio,
#    so the Cloudflare-MITM CA fix is active in sandboxes.
./bin/clang-format-fix                            # formatting
./bin/pio check                                   # cppcheck
./bin/pio run -e default                          # build (default env)
ctest --test-dir build/test --output-on-failure -j  # unit tests
```

If `bin/pio` is not available, set
`PLATFORMIO_CORE_DIR` and `PYTHONPATH` manually (see §3 below).

---

## 1. The CI matrix we reproduce

`/.github/workflows/ci.yml` runs four jobs on every PR. We run the
same four checks in the sandbox:

| GH Actions job | Local equivalent | What it does |
|---|---|---|
| `clang-format`   | `./bin/clang-format-fix` | runs `clang-format-21 -i` on every tracked `*.{c,cpp,h,hpp}` and fails if `git diff` is non-empty |
| `cppcheck`       | `./bin/pio check`        | static analysis; CI flags low/medium/high defects |
| `build`          | `./bin/pio run -e default` (or `gh_release`) | produces `firmware.bin` for ESP32-C3 |
| `unit-tests`     | `ctest --test-dir build/test --output-on-failure -j` | host-side gtest (no hardware needed) |

A "PR ready" merge on `rainmaker-sync` requires **all four to be green**
plus `pr-formatting-check` (PR title linter — not reproduced here).

The default env is what the CI `build` job uploads. The `gh_release`
env is what `release.yml` uploads. Both produce functionally identical
binaries; the only difference is `-DCROSSPOINT_VERSION` baked in.

---

## 2. Sandbox baseline (Ubuntu 22.04, jammy)

The cloud-agent sandbox starts as a minimal Ubuntu 22.04 image. The
following are **not** installed by default and need to be added before
anything else:

```bash
apt-get update
apt-get install -y sudo python3-pip python3.10-venv ca-certificates \
                   g++ cmake ninja-build
```

`apt-get install -y sudo` is needed because `bin/install-deps.sh`
prefixes its `apt` invocations with `sudo`. Without it, the script
exits on the first `sudo apt-get update`.

`clang-format-21` is **not** in the default jammy repo. The cleanest
way to get it is the LLVM apt repo (see `build-and-flash.md` §6.1);
the second-cleanest is `pip3 install 'clang-format>=21,<22'`. The
repo's `bin/install-deps.sh` does the pip route.

`platformio`, `esptool`, `bblanchon/ArduinoJson`, etc. are not in
jammy at all — they come from PyPI and the PlatformIO library registry.

---

## 3. The Cloudflare-MITM / github.com problem (and fix)

### 3.1 What goes wrong

In a sandboxed cloud-agent environment, GitHub release downloads are
served by a Cloudflare MITM proxy whose certificate chain ends in a
self-signed root (`/etc/cloudflare/certs/cloudflare-containers-ca.crt`)
that is **not** in the public CA bundle. This breaks two things:

1. `curl` and Python `requests` will fail with
   `SSL: CERTIFICATE_VERIFY_FAILED`.
2. `pio` will fail with the same error when downloading
   `framework-arduinoespressif32` etc.

### 3.2 The fix that does NOT work

Setting `REQUESTS_CA_BUNDLE` (or `SSL_CERT_FILE`, or `CURL_CA_BUNDLE`)
does **not** help. `bin/install-deps.sh` already builds
`${PLATFORMIO_CORE_DIR}/combined-ca.crt` (system CAs + the
Cloudflare root), but PlatformIO's `HTTPSession` ignores those env
vars and only trusts `certifi.where()`.

`bin/install-deps.sh` even says so:

> PlatformIO's HTTPSession ignores REQUESTS_CA_BUNDLE; only the cert
> that certifi bundles (or the one curl/python ssl trust) sees the
> right chain.

### 3.3 The fix that works

`bin/sitecustomize.py` (auto-loaded by Python at startup when its
parent dir is on `sys.path`) monkey-patches `certifi.where()` to
return the combined CA bundle. The activation recipe:

```bash
export PLATFORMIO_CORE_DIR="$REPO_ROOT/.pio-platformio"
export PLATFORMIO_COMBINED_CA="$PLATFORMIO_CORE_DIR/combined-ca.crt"
export PYTHONPATH="$REPO_ROOT/bin${PYTHONPATH:+:$PYTHONPATH}"
pio run -e default
```

Or just use the wrapper:

```bash
./bin/pio run -e default
```

`./bin/pio` does all of the above. It is a no-op on hosts where the
combined CA bundle is missing (so the same wrapper works on a regular
Linux/macOS dev box without modification).

### 3.4 Verifying the patch is active

```bash
PYTHONPATH=./bin \
PLATFORMIO_COMBINED_CA=./.pio-platformio/combined-ca.crt \
python3 -c "import certifi; print(certifi.where())"
```

Expected output ends with `combined-ca.crt`. If you see the system
`cacert.pem` path instead, the patch is not active — usually because
either `PLATFORMIO_COMBINED_CA` is unset or the file does not exist.

---

## 4. The 504 / intermittent-download problem

Even with SSL fixed, the Cloudflare proxy occasionally returns
`HTTP/1.1 504 Gateway Timeout` for individual `github.com` release
files. Reproduced: `curl -I` to the same URL bounces between 504 and
302 on consecutive calls within seconds.

This is a sandbox-only artifact. On real CI runners, GitHub is
reached directly and 504s don't happen.

### 4.1 Mitigation: retry with pre-warm

A small `pio-retry.sh` wrapper handles this:

```bash
#!/usr/bin/env bash
# Retry `pio run` on cloudflare 504s. Pre-warms failing URLs with
# curl between attempts.
set -uo pipefail

PIO_DIR="$REPO_ROOT/.pio-platformio"
SCRATCH="/tmp/pio-cache"
CA_BUNDLE="$PIO_DIR/combined-ca.crt"
mkdir -p "$SCRATCH"

export PLATFORMIO_CORE_DIR="$PIO_DIR"
export PYTHONPATH="$REPO_ROOT/bin:${PYTHONPATH:-}"

warm() {
    local url="$1"
    for i in 1 2 3 4 5 6 7 8 9 10; do
        code=$(curl -sI -L --max-time 60 --cacert "$CA_BUNDLE" \
               -o /dev/null -w "%{http_code}" "$url" || echo "000")
        if [[ "$code" == "200" || "$code" == "302" || "$code" == "301" ]]; then
            return 0
        fi
        sleep $((i * 3))
    done
    return 1
}

for attempt in 1 2 3 4 5 6 7 8; do
    log="$SCRATCH/pio-${attempt}.log"
    if pio run "$@" 2>&1 | tee "$log"; then
        [[ -f .pio/build/default/firmware.bin ]] && exit 0
    fi
    failing=$(grep -oE 'https://[^ ]+\.(tar\.xz|zip|gz|tar\.bz2|tar)' \
              "$log" | head -1 || true)
    [[ -n "$failing" ]] && warm "$failing"
    sleep $((attempt * 5))
done
exit 1
```

In practice 1-2 attempts are enough; the 504 rarely persists for
more than 10-15 seconds for a given URL.

### 4.2 Mitigation: project-local PlatformIO state

Default `PLATFORMIO_CORE_DIR` is `~/.platformio`. In the sandbox
that path is on a read-only or reset-on-reboot filesystem, so the
PlatformIO package cache gets wiped between sessions. The fix is to
keep PlatformIO state inside the workspace:

```bash
export PLATFORMIO_CORE_DIR="$REPO_ROOT/.pio-platformio"
```

`./bin/pio` does this by default. `bin/install-deps.sh` writes
the combined CA bundle to the same dir.

---

## 5. First-build duration

The first `pio run` in a fresh sandbox pulls roughly:

| Package | Size | Source |
|---|---|---|
| `platform-espressif32@55.3.37` | ~80 MB | pioarduino GitHub release |
| `framework-arduinoespressif32@3.3.7` (+ libs) | ~150 MB | espressif GitHub release |
| `toolchain-riscv32-esp` | ~120 MB | pioarduino GitHub release |
| `tool-scons@4.40801.0` | ~30 MB | PlatformIO registry |
| `lib_deps` (ArduinoJson, QRCode, PNGdec, JPEGDEC, WebSockets) | ~5 MB | PlatformIO library registry |
| **Total** | **~400 MB** | |

On a typical cloud-agent link (50-200 MB/s) that's 30-90 seconds of
download plus 60-90 seconds of compile. Subsequent incremental builds
are 2-3 minutes.

If the build dies mid-way, re-running `pio run` resumes from cache —
the partial state is preserved.

---

## 6. Verifying a built `firmware.bin` without flashing

Use `esptool.py image-info` (needs `rich-click` and `intelhex`):

```bash
pip3 install rich-click intelhex esptool
esptool.py --chip esp32-c3 image-info .pio/build/default/firmware.bin
```

A correctly built image reports:

```
Chip ID: 5 (ESP32-C3)
Flash size: 16MB, freq 80m, mode DIO
Image version: 1
Segments: 7
Checksum: 0x.. (valid)
Validation hash: ... (valid)
```

If any of those lines says `invalid` or `Chip ID` is not `5`, **the
build is corrupt and must not be flashed**. Most common cause: the
build was killed mid-link; re-run `pio run` to finish it.

For the `gh_release` env, also check the three separate
artifacts (`bootloader.bin`, `partitions.bin`, `firmware.bin`) all
exist and the `firmware.bin` image-info is clean.

---

## 7. Differences from GitHub Actions that are intentional

| | GH Actions | Sandbox |
|---|---|---|
| Network egress | direct to github.com | via Cloudflare MITM proxy |
| `PLATFORMIO_CORE_DIR` | `~/.platformio` | `${REPO_ROOT}/.pio-platformio` (sandbox reset-safe) |
| `REQUESTS_CA_BUNDLE` | unused | unused (we patch certifi instead) |
| `pip` install path | system | system (works because we are root) |
| `sudo` | n/a (not needed) | not present by default; install first |
| `clang-format` version | 21 (from LLVM apt) | 21 (from pip, per `bin/install-deps.sh`) |

The functional CI checks produce identical results either way; only
the network plumbing differs.

---

## 8. Gotchas checklist for future agents

- [ ] `bin/install-deps.sh` failed with `sudo: command not found` →
  run `apt-get install -y sudo` first.
- [ ] `pio` not found after install → `which pio` should return
  `/usr/local/bin/pio`. If empty, `pip3 install platformio` was
  wiped; reinstall.
- [ ] `bin/pio` wrapper says `'pio' not found` → same as above.
- [ ] SSL error on `pio run` and no `combined-ca.crt` exists →
  re-run `bin/install-deps.sh`.
- [ ] SSL error even with `combined-ca.crt` present → `bin/pio` is
  not being used; raw `pio` ignores `REQUESTS_CA_BUNDLE`.
- [ ] `504 Gateway Timeout` from the proxy → retry, or use
  `pio-retry.sh` from §4.1.
- [ ] `failed to install Python dependencies into penv` → the
  `penv` virtualenv got partially created; deleting
  `${PLATFORMIO_CORE_DIR}/penv` and re-running usually fixes it.
- [ ] Submodule `freeink-sdk` is empty after rebase/checkout →
  `git submodule update --init --recursive` to re-clone.
- [ ] `firmware.bin` reports invalid checksum → re-run `pio run`;
  the link was probably interrupted.
- [ ] `esptool.py: command not found` when running `image-info` →
  `pip3 install esptool rich-click intelhex` (the last two are
  esptool's optional runtime deps).

---

## 9. See also

- [`build-and-flash.md`](build-and-flash.md) — user-facing build
  and flash guide.
- [`CONTEXT.md`](../CONTEXT.md) — repo conventions and file allow-list.
- [`README.md`](../README.md) — high-level project overview.
- [`/.github/workflows/ci.yml`](../.github/workflows/ci.yml) — the
  exact matrix we reproduce.
