# FoloToy EVA Music Player

[简体中文](README.zh_CN.md) | English

Offline music player firmware for the FoloToy AI Passport (ESP32-C3, 8 MB Flash). Current source version: `1.2.0-dev`. The NERV splash plays a locally prepared startup sound. Songs live in a dedicated Flash partition and can be uploaded from a phone through the device's open Wi-Fi hotspot.

## Use

1. The backlight stays off until the NERV splash is drawn; the startup sound plays with the splash. An empty library shows `NO TRACKS`.
2. While stopped or paused, hold `DOWN` to open transfer mode.
3. Connect to the displayed `EVA-PLAYER-XXXX` hotspot without a password and open `http://192.168.4.1/`.
4. Select a song and wait for the success message. Hold `DOWN` on the device to leave transfer mode. `OK` plays/pauses; `UP` and `DOWN` change songs.

Hold `UP` on the player to open the EVA volume screen. Click `UP` or `DOWN` to adjust output by 5% from 0 to 100%, then click `OK` to return. The setting is saved when leaving the screen and restored at boot. Hold `DOWN` as before to enter and leave hotspot mode.

The page converts music in the browser to mono FAM1/IMA ADPCM at 12 kHz or 8 kHz. Limits: six minutes per song, 30 MB source file, 2.5 MB converted file, and 32 catalog entries. Available Flash space may allow fewer songs. The page can list and delete songs and resend a title image. Playback is paused during transfer mode; after leaving, the selected song starts from the beginning.

The fixed device labels, buttons, and runtime ASCII text are generated locally from a font supplied by the builder. The upload page embeds a compact subset of that font, loads it automatically on every phone, and uses it for page text and new song title masks. No font selection is required on the phone. Characters absent from the subset use the browser's fallback font, and the page reports them when syncing a title. Existing title images need to be resent with “同步歌名”. The font, generated glyphs, and WOFF subset are excluded from Git.

Holding `OK` while paused opens the NERV standby screen; clicking `OK` returns to the player. At the end of a song, the display holds for about two seconds before playing the next song.

The red marks beside “INTERNAL” indicate CW2017 battery charge: one at 1–33%, two at 34–66%, and three at 67–100%. They stay dark at 0% or if the reading fails. The display refreshes this roughly every 30 seconds.

## Build

Use ESP-IDF 5.5.x. Supply your own font with Chinese/Japanese glyph coverage and a legally usable startup sound. Install Python Pillow and fonttools in `.venv` and Node.js/npm, then generate the local font assets before building; see [asset preparation](docs/ASSET_PREPARATION.md). These generated files are excluded from Git and must be recreated after cloning:

```powershell
py -m venv .venv
& '.\.venv\Scripts\python.exe' -m pip install pillow fonttools
.\tools\prepare_local_font.ps1 -FontFile 'C:\path\to\your-font.otf'
.\tools\prepare_startup_audio.ps1 -InputFile 'C:\path\to\startup.mp3'
idf.py build
```

The font and web redesign built with ESP-IDF 5.5.4. The app image is `0x19d530` bytes and leaves about 19% of the 2 MB app slot free. The redesigned page passed desktop and mobile browser preview checks. On 2026-09-18, COM14's 8 MB ESP32-C3 Flash was erased, then the current bootloader, partition table, and app were written with hash verification. Boot logs confirm `Storage=1`, display initialization, audio codec startup, and battery polling. The screen appearance and upload flow have not yet been checked on this fresh flash. A full erase removes stored songs and settings.

A complete 8 MB local image of this build is at `release/FoloToy-EVA-music-player-full-8MB-Matisse-Web-20260918.bin`, with a matching `.sha256` file. Flash the image at offset `0x0`; it fills unused regions with `0xFF` and therefore replaces stored songs and settings. It has been byte-checked against the current bootloader, partition table, and app, but the merged image itself has not been flashed. Do not distribute it without checking rights to its embedded audio, images, and font subset.

Do not publish startup audio, user music, or a firmware image containing protected material without the required rights. See [NOTICE.md](NOTICE.md) and [docs/ASSET_PREPARATION.md](docs/ASSET_PREPARATION.md).

## Layout

| Path | Purpose |
| --- | --- |
| `components/bsp/` | Display, buttons, audio and other board support |
| `main/` | Player, FAM1 decoder, catalog, Wi-Fi service and upload page |
| `assets/audio/` | Local generated startup sound, excluded from Git |
| `tools/prepare_local_font.ps1` | Generates untracked font glyphs, UI masks, and upload page |
| `assets/promo/` | Three 3:4 promotional images based on user-provided device photos; generated artwork, not literal hardware screenshots |
| `partitions.csv` | 2 MB app and approximately 5.9 MB music partition |
| `tests/` | Host-side state and codec tests |

See [ARCHITECTURE.md](ARCHITECTURE.md) for design details. An earlier build was validated on COM14: startup sound, three uploaded songs, correct titles, persistence after reboot, repeated hotspot access, volume controls, and prompt NERV splash. That three-song catalog scan took about 13 seconds under the splash. The latest `1.2.0-dev` font and page build was flashed after a whole-chip erase; boot logs passed, while its screen appearance and upload flow await another on-device check. The local acceptance record is kept outside Git.
