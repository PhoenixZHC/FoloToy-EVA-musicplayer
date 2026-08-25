# Asset Preparation

This page explains how local assets are prepared for the player. It is written so the public repository can avoid distributing protected music and logo files.

## Audio

The firmware expects three generated files:

```text
assets/audio/track0.adpcm
assets/audio/track1.adpcm
assets/audio/track2.adpcm
```

Current intended display names:

| Track | Display name |
| ---: | --- |
| 0 | `残酷な天使のテーゼ` |
| 1 | `One Last Kiss` |
| 2 | `Beautiful World` |

Use only audio that you are legally allowed to use on your own device. Do not publish the source songs or generated ADPCM files unless you have redistribution rights.

The current local conversion target is:

| Parameter | Value |
| --- | --- |
| Channels | mono |
| Sample rate | 8000 Hz |
| PCM input | signed 16-bit |
| Firmware format | custom IMA ADPCM |
| Bit depth after compression | 4-bit ADPCM |

This is intentionally low quality. The reason is simple: the board has 8 MB Flash, no external storage, and no PSRAM.

Local conversion command:

```powershell
.\tools\prepare_audio.ps1
```

The script converts local music files into `.adpcm`. If your file names differ, adjust the script or replace the local files before running it.

## Text Assets

The device does not load a full Japanese font at runtime. Instead, Japanese labels and track names are rendered on the computer into small image assets.

Current generated text assets include:

- top labels such as `EVANGELION`, `再生時間`, `内部`, `音楽再生`, `システム`
- button labels such as `PREV`, `PLAY`, `NEXT`, `PAUSE`
- track titles

The project used a locally installed serif CJK font as a Mincho-style substitute. It does not bundle the licensed EVA font.

Regenerate text assets locally with:

```powershell
py tools/prepare_text_assets.py
py tools/prepare_text_buttons.py
```

If your Windows machine does not have the same font installed, install a suitable redistributable CJK serif font or update the script path.

## Logo Asset

The boot screen uses a generated RGB565 image asset in:

```text
main/eva_logo_assets.c
main/eva_logo_assets.h
```

For this project, the generated logo `.c` and `.h` files are committed with the source so the UI can build consistently.

Generate a local logo asset from your own legal source image:

```powershell
py tools/prepare_nerv_logo.py path\to\logo.png
```

The converter keeps red logo pixels and turns the rest black so the boot screen remains black with a red centered mark.

## Firmware Images

A merged `.bin` image includes the application and embedded assets. If the embedded music is protected, the merged image is also not safe to publish.

For public releases, prefer source code plus instructions. Share a binary only when every embedded asset is redistributable.
