# Asset Preparation

The source repository excludes locally generated audio, font files, font-derived UI assets, and firmware images containing them. Supply your own materials for local builds. Check redistribution rights separately before publishing a binary.

## Startup sound

The firmware embeds one local EVA1/IMA ADPCM startup clip. Generate it from a legally usable source:

```powershell
.\tools\prepare_startup_audio.ps1 -InputFile 'C:\path\to\startup.mp3'
```

The script needs Python 3.13 and FFmpeg (or `imageio_ffmpeg` installed for Python 3.13). It creates `assets/audio/startup.adpcm` at 8 kHz, mono. This file is ignored by Git. The provided local MP3 is only an input for this checkout; it is not part of the source release.

## User songs

Songs are uploaded through the player's open Wi-Fi hotspot, not embedded in the firmware. The browser decodes the selected source file, converts it to 12 kHz or 8 kHz mono, encodes FAM1/IMA ADPCM, and sends it directly to the device. The browser also renders a CJK title as an A8 image. See [FoloToy Gallery](https://github.com/PhoenixZHC/folotoy_gallery) for the underlying format and catalog approach.

## UI font and upload page

Bring an OTF or TTF font you are permitted to use locally. It needs the Chinese and Japanese characters displayed in the player and upload page; a font missing these glyphs will produce missing text. The source repository does **not** contain a font or its generated glyph data. From the repository root, with Python 3.13, Node.js/npm, and ESP-IDF installed:

```powershell
py -m venv .venv
& '.\.venv\Scripts\python.exe' -m pip install pillow fonttools
.\tools\prepare_local_font.ps1 -FontFile 'C:\path\to\your-font.otf'
```

The script generates fixed UI masks, 14 px and 20 px ASCII LVGL font subsets, and the compressed upload page. It uses `lv_font_conv@1.5.3` through `npx`. The internal C symbol names still contain `matisse` for compatibility, but the actual shapes come from the font you supplied. The generated files are `main/eva_text_assets.c/.h`, `main/eva_text_buttons.c/.h`, `main/eva_font_matisse_14.c`, `main/eva_font_matisse_20.c`, and `main/web_ui.h`. All are excluded from Git. The page header contains a WOFF subset, CSS, JavaScript encoder, and NERV logo. Run the script again after changing `main/web_ui.html`, `main/web_style.css`, or `main/audio_adpcm.js`. Previously uploaded titles require “同步歌名” to be rendered again.

The generated font files stay local even when the chosen font permits redistribution. A fresh checkout needs to regenerate them before `idf.py build`; CMake reports a missing local asset if they are absent. `main/eva_logo_assets.c` is a separate image source.

## Firmware images

A merged `.bin` includes the embedded startup sound and generated UI images. Do not redistribute protected assets or a binary containing them without the required rights.
