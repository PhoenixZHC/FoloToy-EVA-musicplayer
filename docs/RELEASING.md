# Releasing

Use this checklist before publishing the project or attaching build artifacts.

## Source Release

Before pushing to a public repository:

1. Confirm no protected music files are tracked.
2. Confirm no generated `.adpcm` files are tracked unless they are redistributable.
3. Confirm generated image assets are included if the UI needs them.
4. Confirm no merged firmware image with protected audio is tracked.
5. Re-read [NOTICE.md](../NOTICE.md).
6. Re-read [GIT_RULES.md](GIT_RULES.md).

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
release/*.bin
```

## Build Release

Only create and publish a full firmware image when every embedded asset can be redistributed.

The merged image layout is:

| Offset | File |
| ---: | --- |
| `0x0` | bootloader |
| `0x8000` | partition table |
| `0x10000` | application |

The local project name used for the verified binary was:

```text
FoloToy-EVA-music-player-full.bin
```

## Hardware Release Notes

When publishing a tested firmware version, include:

- board name and revision if known
- ESP-IDF version
- build result
- host test result
- device port used for testing
- whether startup, display, buttons, audio, auto-next, and pause/play were verified
- whether the binary includes non-redistributable local assets
