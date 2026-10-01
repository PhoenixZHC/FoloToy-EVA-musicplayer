# Releasing

Use this checklist before publishing the project or attaching build artifacts.

## Source Release

Before pushing to a public repository:

1. Confirm no protected music files are tracked.
2. Confirm no generated `.adpcm` files are tracked unless they are redistributable.
3. Confirm generated font assets and `web_ui.h` are excluded; README explains how builders make them locally.
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
main/web_ui.h
release/*.bin
```

## Build Release

Only publish a full firmware image when every embedded asset can be redistributed. The current source identifies itself as `1.2.0-dev`, not a final numbered release.

The merged image layout is:

| Offset | File |
| ---: | --- |
| `0x0` | bootloader |
| `0x8000` | partition table |
| `0x10000` | application |
| `0x210000` | writable music FAT partition (populated in preset-song packages) |

The current local package is `release/FoloToy-EVA-player-3songs-fixed-20261001.bin` (8,388,608 bytes, flash at `0x0`). It includes three deletable preset tracks and the catalog revision, interrupted deletion, and auto-next fixes. Its SHA-256 is `2f11d027f0805997c09b483c72579e7bd39b86a7231760d71e3a93e5df470413`. The complete image has been checked against its build segments and music partition. It was written to COM14 on 2026-10-01 using ESP-IDF 5.5.4 / esptool 4.12.0: ESP32-C3 revision v1.1, 8 MB Flash, data hash verified, exit code 0, hardware reset issued. Playback, UI, multiple-phone use, real power loss, and runtime heap remain unverified for this build. Flashing the full image replaces existing songs and settings.

The local `release/` directory is excluded from Git. For normal full-image flashing, use only the complete `fixed` BIN:

| File suffix/name | Purpose |
| --- | --- |
| `FoloToy-EVA-player-3songs-fixed-20261001.bin` | Current complete firmware, including music; flash at `0x0` |
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
- whether the current font, mobile upload page, song titles, repeat AP access, and persistence were verified on the flashed build
- whether the binary includes non-redistributable local assets
