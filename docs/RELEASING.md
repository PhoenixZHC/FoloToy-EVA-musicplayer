# Releasing

Use this checklist before publishing the project or attaching build artifacts.

## Source Release

Before pushing to a public repository:

1. Confirm no protected music files are tracked.
2. Confirm no generated `.adpcm` files are tracked unless they are redistributable.
3. Confirm generated Matisse and Chinese radio font assets and `web_ui.h` are excluded; README explains how builders make them locally.
4. Confirm no merged firmware image with protected audio is tracked.
5. Check the source and rights for promotional art; the font and its derivatives stay local.
6. Re-read [NOTICE.md](../NOTICE.md).
7. Re-read [GIT_RULES.md](GIT_RULES.md).

Suggested local checks:

```powershell
git status --short
git ls-files
```

Look especially for:

```text
*.mp3
*.wav
*.flac
*.m4a
assets/audio/*.adpcm
assets/preset_music/
*.fam
main/eva_text_assets.c
main/eva_text_buttons.c
main/eva_font_matisse_*.c
main/eva_font_radio_16.c
main/eva_font_radio_20.c
assets/fonts/
main/web_ui.h
release/*.bin
```

## Current Source Snapshot (2026-11-01)

Version remains 1.2.0-dev. Current navigation: long UP switches and persists local/radio mode, long DOWN opens/leaves the EVA hotspot, long OK opens/saves settings, and double OK on radio opens city/station selection. Volume, brightness, screen-off and read-only firmware version are in settings. Radio remains 19 cities/89 sampled streams.

Full checks passed: Python 32 passed/1 skipped (three private preset songs absent), 9 standalone C programs and 3 JavaScript scripts, ESP-IDF 5.5.4 build/size. The app is 1999984 bytes (0x1e8470), with 97168 bytes free in its 2 MiB partition and 112356 bytes of static DRAM remaining. Application-only flash/hash verification passed, preserving NVS/music. Current serial checks and their limits are in [November 1 evidence](HARDWARE_TEST_20261101.md); earlier acceptance stays [historical](HARDWARE_TEST_20261030.md). Do not claim the new layout or audible continuity from logs alone.

Review staged source/docs and the destination branch, then push without force. Keep release/, local media/fonts, raw logs and device backups excluded. User authorization to publish source does not automatically authorize publicly redistributing embedded assets or reflashing hardware.

## Build Release

Only publish a full firmware image when every embedded asset can be redistributed. The current source identifies itself as `1.2.0-dev`, not a final numbered release.

The merged image layout is:

| Offset | File |
| ---: | --- |
| `0x0` | bootloader |
| `0x8000` | partition table |
| `0x10000` | application |
| `0x210000` | writable music FAT partition (populated in preset-song packages) |

### Current Local Full Image (2026-11-01)

- File: release/FoloToy-EVA-1.2.0-dev-full-8MB-20261101.bin.
- Length: 8388608 bytes (8 MiB); flash at 0x0 on ESP32-C3 / 8MB / DIO / 80MHz.
- SHA-256: aa990bc81ca76d64252a82a4d316d0c7d8f0507acf819c406959aa5a222be795.
- Contents: current bootloader, partition table and the same application that passed device flash/hash checks. NVS (0x9000–0xefff), PHY (0xf000–0xffff) and music (0x210000–0x7fffff) are 0xFF. No user Wi-Fi credentials, settings or songs are included; music is initialized through existing first-boot logic.
- Validation: exact length, SHA-256, byte-for-byte component comparisons and blank-region checks passed. The complete merged file was not flashed again; application-only device testing is separate evidence.
- Local companions: same-name .sha256 and .json, plus release/交付说明.txt. These are not flash inputs and remain Git-ignored with the BIN.

**Flashing the full image replaces existing songs and settings.** For an upgrade that preserves data, use the application at 0x10000 instead. This packaging task did not erase or reflash the existing device.

To reproduce after preparing assets and building, run from the repository root in Windows PowerShell with ESP-IDF active:

~~~powershell
New-Item -ItemType Directory -Path 'release' -Force | Out-Null
if (Test-Path -LiteralPath 'release/FoloToy-EVA-1.2.0-dev-full-8MB-20261101.bin') { throw 'Choose a new output name; the delivery already exists' }
$espPython = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
$mergeArgs = @(
    '--chip', 'esp32c3', 'merge_bin', '--output', 'release/FoloToy-EVA-1.2.0-dev-full-8MB-20261101.bin',
    '--flash_mode', 'dio', '--flash_freq', '80m', '--flash_size', '8MB', '--fill-flash-size', '8MB',
    '0x0', 'build/bootloader/bootloader.bin',
    '0x8000', 'build/partition_table/partition-table.bin',
    '0x10000', 'build/FoloToy-AI-Passport.bin'
)
& $espPython -m esptool @mergeArgs
if ($LASTEXITCODE -ne 0) { throw 'Full-image merge failed' }
Get-FileHash -Algorithm SHA256 -LiteralPath 'release/FoloToy-EVA-1.2.0-dev-full-8MB-20261101.bin'
~~~

On Linux, use the active ESP-IDF environment's bin/python with the same esptool arguments; the project .venv is a separate tools/test environment. Do not overwrite a previous delivery file; choose a new dated name if inputs change. The recorded hash applies only to the exact local inputs above. Optional preset-song packages still use tools/package_factory.py; they require real media and are not the clean package described here.

### Historical October 1 Package (No Radio)

The recorded local package was `release/FoloToy-EVA-player-3songs-fixed-20261001.bin` (8,388,608 bytes, flash at `0x0`). It includes three deletable preset tracks and the catalog revision, interrupted deletion, and auto-next fixes. Its SHA-256 is `2f11d027f0805997c09b483c72579e7bd39b86a7231760d71e3a93e5df470413`. The complete image has been checked against its build segments and music partition. It was written to COM14 on 2026-10-01 using ESP-IDF 5.5.4 / esptool 4.12.0: ESP32-C3 revision v1.1, 8 MB Flash, data hash verified, exit code 0, hardware reset issued. Playback, UI, multiple-phone use, real power loss, and runtime heap remain unverified for this build. Flashing the full image replaces existing songs and settings.

The local `release/` directory is excluded from Git. For normal full-image flashing, use only the complete `fixed` BIN:

| File suffix/name | Purpose |
| --- | --- |
| `FoloToy-EVA-player-3songs-fixed-20261001.bin` | Historical complete firmware, including music; flash at `0x0` |
| `FoloToy-EVA-player-3songs-20261001.bin` | Historical package before the three fixes |
| `.music.bin` | Music partition only; not a complete firmware image |
| `.json`, `.sha256` | Packaging report and source-file checksum; not flash inputs |

Preparation and packaging commands are in [Asset Preparation](ASSET_PREPARATION.md). Check the input file's SHA-256 before flashing. Esptool may update bootloader header fields and their digest to match the selected Flash settings; its write verification applies to the bytes actually sent.

### Historical packages

Historical local image names for the old fixed-song `1.1.0` firmware were:

```text
FoloToy-EVA-music-player-full.bin
```

```text
FoloToy-EVA-music-player-full-v1.1.0.bin
```

The earlier Matisse font and web redesign was packaged as `release/FoloToy-EVA-music-player-full-8MB-Matisse-Web-20260918.bin`. It had blank `0xFF` NVS, PHY, and music regions. Its populated sections were compared byte for byte with that build. The merged image itself was not flashed; its bootloader, partition table, and app were flashed separately after a whole-chip erase. These historical names are not download links or confirmation that the files are still present locally.

## Hardware Release Notes

When publishing a tested firmware version, include:

- board name and revision if known
- ESP-IDF version
- build result
- host test result
- device port used for testing
- whether startup, display, buttons, audio, auto-next, and pause/play were verified
- radio source count/date, city navigation, network setup, settings and mode switching, long-running/cross-network audio, and maximum-volume distortion acceptance
- whether the current font, mobile upload page, song titles, repeat AP access, and persistence were verified on the flashed build
- whether the binary includes non-redistributable local assets
