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

Historical local image names for the old fixed-song `1.1.0` firmware were:

```text
FoloToy-EVA-music-player-full.bin
```

```text
FoloToy-EVA-music-player-full-v1.1.0.bin
```

The local `1.2.0-dev` 8 MB image for the Matisse font and web redesign is `release/FoloToy-EVA-music-player-full-8MB-Matisse-Web-20260918.bin`. Its `.sha256` file records the checksum. The image starts at `0x0` and has blank `0xFF` NVS, PHY, and music regions. Its populated sections were compared byte for byte with the build. The merged image itself has not been flashed; the corresponding bootloader, partition table, and app were flashed separately after a whole-chip erase. A full image at `0x0` erases stored songs and settings. Keep this local image out of the repository unless rights for its audio, logos, and font are confirmed.

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
