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

Complete factory images can also include preset songs as ordinary files in the same writable music partition. They appear immediately in the library and can be deleted from music management, freeing their storage. Deleted presets stay deleted after reboot; reflashing the complete factory image reinstalls them. See [deletable factory songs](docs/ASSET_PREPARATION.md#optional-deletable-factory-songs) for preparation and packaging. An application-only flash does not install presets.

When another page changes the library, stale delete/title-sync requests are rejected and the list refreshes; select the song again to proceed. Deleting a song no longer renames the remaining files. Boot accepts empty slots, and uploads reuse them (so a new song may appear earlier in the list). If deletion stops after removing only the title mask, the song remains playable and “同步歌名” restores its title. Holding `UP` during the end-of-song pause retains automatic next-track playback.

The fixed device labels, buttons, and runtime ASCII text are generated locally from a font supplied by the builder. The upload page embeds a compact subset of that font, loads it automatically on every phone, and uses it for page text and new song title masks. No font selection is required on the phone. Characters absent from the subset use the browser's fallback font, and the page reports them when syncing a title. Existing title images need to be resent with “同步歌名”. The font, generated glyphs, and WOFF subset are excluded from Git.

Holding `OK` while paused opens the NERV standby screen; clicking `OK` returns to the player. At the end of a song, the display holds for about two seconds before playing the next song.

The red marks beside “INTERNAL” indicate CW2017 battery charge: one at 1–33%, two at 34–66%, and three at 67–100%. They stay dark at 0% or if the reading fails. The display refreshes this roughly every 30 seconds.

## Build

The current local complete image is `release/FoloToy-EVA-player-3songs-fixed-20261001.bin` (8 MiB, flash at `0x0`, replaces existing songs/settings). It includes the bootloader, partition table, application, and three deletable presets: `残酷な天使のテーゼ（TV版）`, `Beautiful World`, and `One Last Kiss`. The similarly named `.music.bin` is only the music partition; the image without `fixed` predates the three fixes. Use only the complete `fixed` BIN for a full flash. These generated files are not included in a source checkout.

ESP-IDF 5.5.4 built app `0x19a2d0` with 417,072 bytes (20%) free in its partition; the preset music volume has 1,978,368 bytes free. Host regression tests and final package checks passed. On 2026-10-01 the complete image was written to COM14 (ESP32-C3 revision v1.1, 8 MB Flash), its data hash verified, and a hardware reset issued. This confirms flashing, not playback or UI acceptance. Playback, multiple-phone management, physical power-loss behavior, and runtime heap usage still need device validation. Preparation and packaging commands are in [asset preparation](docs/ASSET_PREPARATION.md).

Use ESP-IDF 5.5.x. Supply your own font with Chinese/Japanese glyph coverage and a legally usable startup sound. Install Python Pillow and fonttools in `.venv` and Node.js/npm, then generate the local font assets before building; see [asset preparation](docs/ASSET_PREPARATION.md). These generated files are excluded from Git and must be recreated after cloning:

```powershell
py -m venv .venv
& '.\.venv\Scripts\python.exe' -m pip install pillow fonttools
.\tools\prepare_local_font.ps1 -FontFile 'C:\path\to\your-font.otf'
.\tools\prepare_startup_audio.ps1 -InputFile 'C:\path\to\startup.mp3'
idf.py build
```

For image layout, checksums, and the distinction between current and historical packages, see [release guidance](docs/RELEASING.md).

Do not publish startup audio, user music, or a firmware image containing protected material without the required rights. See [NOTICE.md](NOTICE.md) and [docs/ASSET_PREPARATION.md](docs/ASSET_PREPARATION.md).

## Layout

| Path | Purpose |
| --- | --- |
| `components/bsp/` | Display, buttons, audio and other board support |
| `main/` | Player, FAM1 decoder, catalog, Wi-Fi service and upload page |
| `assets/audio/` | Local generated startup sound, excluded from Git |
| `assets/preset_music/` | Optional generated FAM1 songs and ETT1 title masks, excluded from Git |
| `tools/prepare_music.py`, `tools/package_factory.py` | Prepare deletable presets and package a complete local image |
| `tools/prepare_local_font.ps1` | Generates untracked font glyphs, UI masks, and upload page |
| `assets/promo/` | Three 3:4 promotional images based on user-provided device photos; generated artwork, not literal hardware screenshots |
| `partitions.csv` | 2 MB app and approximately 5.9 MB music partition |
| `tests/` | Host-side state, codec, catalog, HTTP, browser request, and packaging tests |

See [ARCHITECTURE.md](ARCHITECTURE.md) for design details. September builds were validated on COM14 for startup sound, uploaded songs, titles, persistence, repeated hotspot access, volume controls, and prompt NERV splash. A three-song scan then took about 13 seconds under the splash. Those observations do not establish acceptance of the October build. Local detailed acceptance records stay outside Git.
