# Build & Flash — CrossPoint × Rainmaker (X4 / X3)

This document covers building the firmware from source, flashing it to an
xteink X4 (or X3), and safely falling back to a previous firmware if
something goes wrong.

It is written for **end users** with a working Linux/macOS machine. If you
are a cloud agent or CI maintainer, see [`ci-and-sandbox.md`](ci-and-sandbox.md)
for the sandbox-specific gotchas.

---

## 1. Prerequisites

| Tool | Min version | Why | Install (Ubuntu 22.04) |
|---|---|---|---|
| `git` | any | submodule + branches | `apt install git` |
| `python3` | 3.10 | PlatformIO host | `apt install python3 python3-pip` |
| `platformio` (aka `pio`) | 6.1.x | build orchestrator | `pip3 install platformio` |
| `esptool.py` | 4.x | flashing | `pip3 install esptool` |
| `cmake` + `ninja` | any | host-side unit tests | `apt install cmake ninja-build` |
| `clang-format` | **21** | formatting check (CI) | See [§6.1](#61-clang-format-21-on-ubuntu-2204) |
| `curl` | any | retry helper | `apt install curl` |

The repo's `bin/install-deps.sh` installs everything in one shot on
Ubuntu 22.04 (it is idempotent and only touches `apt` and `pip3`).

> **Do not** use your distro's `platformio` package — always install
> via `pip3 install platformio` so the CLI matches the version in
> `.github/workflows/ci.yml` (6.1.19).

---

## 2. One-shot build (release firmware)

```bash
git clone https://github.com/lychan110/crosspoint-reader.git
cd crosspoint-reader
git checkout rainmaker-sync
./bin/install-deps.sh
git submodule update --init --recursive
pio run -e gh_release
```

You will get five artifacts in `.pio/build/gh_release/`:

| File | Size (≈) | What it is |
|---|---|---|
| `bootloader.bin`  |  19 KB | second-stage bootloader |
| `partitions.bin`  |   3 KB | partition table (matches `partitions.csv`) |
| `firmware.bin`    | 5.0 MB | app image (the CrossPoint+Rainmaker binary) |
| `firmware.elf`    |  52 MB | ELF with debug info — for `addr2line` / GDB |
| `firmware.map`    |  18 MB | link map — for stack/RAM forensics |

For most users **only `firmware.bin` matters** — flash it at offset
`0x10000` together with the bootloader and partition table
(see [§3](#3-flashing-the-x4) below).

If you just want to verify a change compiles and passes static analysis:

```bash
pio run -e default           # faster: no version string baked in
pio check                    # cppcheck
ctest --test-dir build/test --output-on-failure -j
```

---

## 3. Flashing the X4

> **Read §4 first.** A bad flash can leave the device in a state where
> it won't boot. The recovery procedure is simple but requires that
> you have the previous firmware file available.

### 3.1 Identify the serial port

Plug the X4 in over USB and find its `/dev/tty*` device:

```bash
# Linux
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
# macOS
ls /dev/cu.usbmodem*
```

The X4 enumerates as a USB CDC device (no driver install needed on
modern Linux/macOS).

### 3.2 Verify the image before flashing

Always re-verify the freshly built `firmware.bin` before touching flash:

```bash
esptool.py --chip esp32-c3 image-info .pio/build/gh_release/firmware.bin
```

Expected output (abridged):

```
Chip ID: 5 (ESP32-C3)
Flash size: 16MB, freq 80m, mode DIO
Image version: 1
Segments: 7
Checksum: 0x.. (valid)
Validation hash: ... (valid)
```

If `Chip ID` is not `5` or any line says `invalid`, **do not flash**.

### 3.3 The actual flash

```bash
PORT=/dev/ttyACM0                       # adjust for your machine
BASE=.pio/build/gh_release

esptool.py \
    --chip esp32-c3 \
    --port "$PORT" \
    --baud 921600 \
    --before default_reset \
    --after hard_reset \
    write_flash \
    --flash_mode dio --flash_size 16MB --flash_freq 80m \
    0x0      "$BASE/bootloader.bin" \
    0x8000   "$BASE/partitions.bin" \
    0x10000  "$BASE/firmware.bin"
```

You should see three `[==]       ]` progress bars and end with
`Hash of data verified`. Power-cycle the device and you should see the
CrossPoint splash screen within ~3 seconds.

### 3.4 First-time setup

After the first boot with a new firmware:

1. **SD card format:** FAT32, 32 GB or smaller recommended. exFAT works
   on most kernels but some SD card readers are flaky with it.
2. **Wi-Fi:** the device's `Join Network` screen (or web UI) lets you
   enter credentials. They are stored in NVS and survive flash updates.
3. **Rainmaker dashboard:** see
   [§5](#5-rainmaker-dashboard) below.

---

## 4. Reversion & safe fallbacks

Flashing a non-bootable firmware is recoverable in two ways. The
mitigations below are **strongly recommended** for every flash session.

### 4.1 Mitigations (do these BEFORE flashing)

1. **Keep the previous firmware on disk.** Save the last `firmware.bin`
   that booted cleanly to a known path:

   ```bash
   mkdir -p ~/x4-firmware-backups
   cp .pio/build/gh_release/firmware.bin \
      ~/x4-firmware-backups/firmware-$(date +%Y%m%d-%H%M%S)-BEFORE.bin
   ```

   After the new build boots cleanly, keep the prior version as the
   "known-good" rollback target.

2. **Snapshot flash contents** (highly recommended, ~30s):

   ```bash
   esptool.py --chip esp32-c3 --port "$PORT" --baud 921600 \
       read_flash 0x0 0x1000000 x4-backup-$(date +%Y%m%d).bin
   ```

   This is the entire 16 MB of flash. You can re-flash it with the same
   `write_flash` command from §3.3 and you are back to exactly where you
   started, byte-for-byte.

3. **Verify the merge offsets** in §3.3 match the `partitions.csv` in
   the source tree. If you ever edit `partitions.csv`, the offsets
   **will change** and an old backup becomes unbootable until you reflash
   at the new offsets.

### 4.2 If the device won't boot after flashing

The X4 has a USB-attached serial bootloader on the ESP32-C3's ROM, so
the chip is *never* truly bricked — it always accepts an esptool flash
even if the application image is corrupt. The flow is:

1. **Hold the boot button** on the X4 (the small tactile switch near
   the USB port) while plugging in the USB cable. This puts the chip
   into download mode.
2. Run the same `esptool.py write_flash` from §3.3 with the *known-good*
   `firmware.bin` you saved in §4.1.
3. Power-cycle. The device should boot normally.

If you don't have a button-press entry into download mode, the ESP32-C3
ROM bootloader auto-enters download mode when it cannot find a valid
app at `0x10000` — so a deliberately empty `0x10000` (or one with a
bad image) is enough. You can force this with:

```bash
esptool.py --chip esp32-c3 --port "$PORT" --baud 921600 \
    erase_flash
```

This wipes the entire 16 MB. After erase, the chip enters download mode
automatically and you can flash fresh.

> **Do not** use `erase_flash` to "start over from a clean slate" if you
> also have an OTA partition (`app1` in `partitions.csv`) with a working
> image — erasing it removes your rollback target. The X4 ships with
> OTA, so the proper recovery is to re-flash the known-good image at
> `0x10000` (and `0x650000` if you used `app1`).

### 4.3 If the device boots but misbehaves

- **Stuck on splash, no UI:** reflashing fixes it. The flash is idempotent.
- **Settings lost:** NVS is at `0x9000`. If it was preserved across the
  flash (it should be — only `app0` is touched) the issue is elsewhere.
- **SD card not detected:** try a different SD card or a different
  reader. The X4's SD card slot is known to be picky with high-capacity
  cards.
- **Wi-Fi credentials forgotten:** the X4 will forget Wi-Fi after a
  full `erase_flash` but not after a normal `write_flash`. Re-enter them
  via the on-device menu.

### 4.4 Pre-flight checklist (recommended for every flash)

```
[ ] Last known-good firmware.bin is on disk in ~/x4-firmware-backups/
[ ] Full 16MB flash dump is saved to x4-backup-<date>.bin
[ ] `esptool.py image-info` shows Chip ID 5, valid Checksum, valid Hash
[ ] Serial port is correct (/dev/ttyACM0 or similar)
[ ] Battery is > 50% (USB can power the device but only the chip, not
    the e-ink refresh — avoid flashing on a near-empty battery)
[ ] If flashing for the first time after editing partitions.csv, the
    offsets in §3.3 match the current partitions.csv
```

---

## 5. Rainmaker dashboard

The `rainmaker-sync` fork adds a sync mode that downloads a dashboard
BMP from a URL you configure and renders it on the sleep screen.

Settings (all under **Settings → Rainmaker** on-device or in the
web UI):

| Setting | Default | What it does |
|---|---|---|
| `rainmakerSyncEnabled` | off | Master switch. First enable auto-sets the sleep screen to `RAINMAKER` for the next sleep cycle. |
| `rainmakerManifestUrl` | empty | HTTPS URL to the manifest JSON. |
| `rainmakerUsername` / `rainmakerPassword` | empty | HTTP Basic auth on the manifest and BMP endpoints. |
| `rainmakerIntervalMinutes` | 30 | Min minutes between scheduled syncs. |
| `rainmakerStartMinutes` | 480 (08:00) | Earliest local-time sync window start. |
| `rainmakerEndMinutes` | 1320 (22:00) | Latest local-time sync window end. |
| `rainmakerMinBatteryPercent` | 20 | Refuse to sync below this. |

The manifest schema is the one in [`README.md`](../README.md) §"MANIFEST
SCHEMA". The sync algorithm (validate → fetch manifest → diff SHA-256
→ download → verify → atomic rename) is fail-safe: a bad fetch
preserves the previous cached BMP, the device just shows a blank
sleep screen.

For local development, you can serve the manifest and BMP from any
HTTPS-capable static host. `curl --basic -u user:pass` should be enough
to verify the endpoints manually.

---

## 6. Troubleshooting the build itself

### 6.1 clang-format 21 on Ubuntu 22.04

`apt install clang-format` only ships version 14 on jammy, but the
repo's `.clang-format` requires 21+. The two ways to get 21:

```bash
# Option A: LLVM's apt repo
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key \
  | gpg --dearmor -o /usr/share/keyrings/llvm.gpg
echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/jammy/ llvm-toolchain-jammy-21 main" \
  > /etc/apt/sources.list.d/llvm.list
sudo apt-get update
sudo apt-get install -y clang-format-21
```

```bash
# Option B: pip (works on any distro, matches CI)
pip3 install 'clang-format>=21,<22'
```

If you have multiple versions installed, ensure
`./bin/clang-format-fix` picks the right one — it auto-selects
`clang-format-21` and falls back to `clang-format`, but it will refuse
to run with anything < 21.

### 6.2 `pio check` reports an error on a clean PR

The CI runs `pio check --fail-on-defect low --fail-on-defect medium
--fail-on-defect high`. Newer versions of cppcheck sometimes add new
defect categories; if you see one on an unchanged file, it's likely a
toolchain upgrade. Run `pio check -v` and file an issue — the rule
suppressions in `platformio.ini`'s `check_flags` are reviewed on every
toolchain bump.

### 6.3 Unit tests fail on a fresh checkout

```bash
cmake -S test -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/test
ctest --test-dir build/test --output-on-failure -j
```

The first `cmake -S test -B build/test` clones GoogleTest via
`FetchContent` (a few hundred KB). If it fails with a network error,
re-run after verifying `https://github.com` is reachable.

### 6.4 Out-of-memory at link time (ESP32-C3 has 320 KB RAM)

The current image uses 31.0% RAM and 79.6% flash. If you add a feature
and the link fails with `region 'iram0_0' overflowed`, see
[`scope-discipline`](../.claude/skills/scope-discipline/SKILL.md) and
[`heap-discipline`](../.claude/skills/heap-discipline/SKILL.md) — the
two biggest offenders are usually `std::vector` and `std::string` in
hot paths.

### 6.5 Build is slow on first run

The first `pio run` downloads:

- `platform-espressif32@55.3.37` (~80 MB) — ESP32 Arduino core
- `framework-arduinoespressif32@3.3.7` (~150 MB) — the toolchain + libs
- `toolchain-riscv32-esp` (~120 MB) — the RISC-V GCC for ESP32-C3
- The five `lib_deps` from `platformio.ini` (~5 MB)

Total first-time download: ~400 MB. Subsequent builds are incremental
(2-3 minutes for a no-op rebuild on a fast machine).

---

## 7. See also

- [`ci-and-sandbox.md`](ci-and-sandbox.md) — agent-facing notes on
  running the same checks in cloud-agent sandboxes.
- [`README.md`](../README.md) — high-level project overview.
- [`docs/plans/crosspoint-fork-implementation-handoff.md`](plans/crosspoint-fork-implementation-handoff.md) —
  authoritative spec for the Rainmaker dashboard sync.
- [`docs/troubleshooting.md`](troubleshooting.md) — on-device issues
  (Wi-Fi, SD card, etc.), unrelated to building/flashing.
