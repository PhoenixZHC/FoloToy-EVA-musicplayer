# Asset Preparation

The source repository excludes locally generated audio, font files, font-derived UI assets, and firmware images containing them. Supply your own materials for local builds. Check redistribution rights separately before publishing a binary.

## Startup sound

The firmware embeds one local EVA1/IMA ADPCM startup clip. Generate it from a legally usable source:

```powershell
.\tools\prepare_startup_audio.ps1 -InputFile 'C:\path\to\startup.mp3'
```

The script needs Python 3.13 and FFmpeg (or `imageio_ffmpeg` installed for Python 3.13). It creates `assets/audio/startup.adpcm` at 8 kHz, mono. This file is ignored by Git. The provided local MP3 is only an input for this checkout; it is not part of the source release.

## User songs

Songs live as ordinary files in the writable FAT partition. They can be uploaded through the player's open Wi-Fi hotspot or included in a complete factory image using the workflow below. The browser decodes the selected source file, converts it to 12 kHz or 8 kHz mono, encodes FAM1/IMA ADPCM, and sends it directly to the device. The browser also renders a CJK title as an A8 image. See [FoloToy Gallery](https://github.com/PhoenixZHC/folotoy_gallery) for the underlying format and catalog approach.

### Optional deletable factory songs

`tools/prepare_music.py` uses FFmpeg and the existing browser encoder (`main/audio_adpcm.js`), plus Pillow/fonttools for 34-pixel-high ETT1 title masks. Run it with a Python environment containing those packages and Node.js on PATH. FFmpeg may be on PATH, supplied with `--ffmpeg`, or provided by `imageio_ffmpeg`. Each `--track` accepts a local audio path and a display title. For example, after preparing `.venv` as below:

```powershell
$musicArgs = @(
    'tools/prepare_music.py', '--font', 'C:\path\to\your-font.otf',
    '--track', 'C:\music\first.mp3', 'First song',
    '--track', 'C:\music\second.mp3', 'Second song',
    '--track', 'C:\music\third.mp3', 'Third song'
)
& '.\.venv\Scripts\python.exe' @musicArgs
```

The default output is Git-ignored `assets/preset_music/`: consecutive `a000.fam` / `t000.bin` pairs. It must be empty before generation; use `--output` with a new directory when changing the playlist. The default rate is 12 kHz; `--sample-rate 8000` explicitly selects a smaller file. Invalid titles, missing glyphs, songs longer than six minutes, and oversized files fail visibly. The script does not silently lower quality or replace missing glyphs.

Activate ESP-IDF 5.5.x, then build and package with its Python interpreter (which includes the FAT generator dependencies):

```powershell
idf.py -B build_preset build
python tools/package_factory.py --build build_preset --output release/EVA-with-music.bin
```

`package_factory.py` reads the built partition table and checks the 8 MB Flash / 4096-byte wear-levelling sector configuration. It generates a writable FAT image using ESP-IDF's `WLFATFS`, extracts every song and title in memory to verify byte equality, and combines it with the built bootloader, partition table, and application. Outputs include the complete 8 MiB image, `.music.bin`, `.sha256`, and a JSON verification report. Existing packages are never overwritten. `--music` selects an alternative prepared directory; `--idf-path` can identify ESP-IDF if not activated.

For the example above, `release/EVA-with-music.bin` is the only file needed for a complete flash. `EVA-with-music.music.bin` contains only the music partition and must not be used as a full image at `0x0`; `.json` and `.sha256` are verification records. See [Releasing](RELEASING.md) for the current local package name and its verification status.

The complete image is intended for flashing at `0x0` and **replaces existing music and settings**. Normal `idf.py flash` only writes the application components and does not install these presets. Presets occupy music storage, not app space or additional playback RAM. The existing music-management page can delete them and reuse their space, including deleting every song. Rebooting or an application-only update does not restore deleted presets. Reflashing the complete factory image restores its initial playlist. This packaging workflow never connects to or flashes a device.

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
