# FoloToy EVA Music Player

[简体中文](README.zh_CN.md) | English

Music player firmware for the FoloToy AI Passport (ESP32-C3, 8 MB Flash), with an integrated internet radio prototype. Current source version: `1.2.0-dev`. The NERV splash plays a locally prepared startup sound. Local songs live in a dedicated Flash partition and can be uploaded from a phone through the device's open Wi-Fi hotspot.

## Use

1. The backlight stays off until the NERV splash is drawn; the startup sound plays with the splash. An empty library shows `NO TRACKS`.
2. From local music or radio, hold `DOWN` to stop playback and open the EVA hotspot screen.
3. Connect to the displayed `EVA-PLAYER-XXXX` hotspot without a password and open `http://192.168.4.1/`.
4. Select a song and wait for the success message. Hold `DOWN` on the device to leave transfer mode. `OK` plays/pauses; `UP` and `DOWN` change songs.

Hold `UP` to switch between local music and internet radio; the mode persists across restarts. Hold `DOWN` to enter or leave the hotspot. Hold `OK` for EVA settings: brightness, volume, screen-off timeout and the actual firmware version. Volume adjusts by 5% from 0 to 100% and saves when leaving settings. The existing codec curve and zero-volume mute are unchanged.

The page converts music in the browser to mono FAM1/IMA ADPCM at 12 kHz or 8 kHz. Limits: six minutes per song, 30 MB source file, 2.5 MB converted file, and 32 catalog entries. Available Flash space may allow fewer songs. The page can list and delete songs and resend a title image. Playback is paused during transfer mode; after leaving, the selected song starts from the beginning.

Complete factory images can also include preset songs as ordinary files in the same writable music partition. They appear immediately in the library and can be deleted from music management, freeing their storage. Deleted presets stay deleted after reboot; reflashing the complete factory image reinstalls them. See [deletable factory songs](docs/ASSET_PREPARATION.md#optional-deletable-factory-songs) for preparation and packaging. An application-only flash does not install presets.

When another page changes the library, stale delete/title-sync requests are rejected and the list refreshes; select the song again to proceed. Deleting a song no longer renames the remaining files. Boot accepts empty slots, and uploads reuse them (so a new song may appear earlier in the list). If deletion stops after removing only the title mask, the song remains playable and “同步歌名” restores its title. Opening settings during the end-of-song pause retains automatic next-track playback; holding `UP` switches source mode.

The fixed device labels, buttons, and runtime ASCII text are generated locally from a font supplied by the builder. The upload page embeds a compact subset of that font, loads it automatically on every phone, and uses it for page text and new song title masks. No font selection is required on the phone. Characters absent from the subset use the browser's fallback font, and the page reports them when syncing a title. Existing title images need to be resent with “同步歌名”. The font, generated glyphs, and WOFF subset are excluded from Git.

Hold OK to open the Chinese settings menu. UP/DOWN selects a row, OK edits/confirms, and holding OK or choosing 保存并返回 saves and applies. 模式 selects local music or radio; 亮度 is 10–100%; 音量 is 0–100%; 版本 shows the actual firmware version; 自动熄屏 is never/15/30/60/120/300 seconds; 网络设置 opens the setup hotspot. Screen-off only disables the backlight, and the first key wakes without triggering another action. Settings and source mode persist across restarts. At song end, playback waits about two seconds before advancing.

The red marks beside “INTERNAL” indicate CW2017 battery charge: one at 1–33%, two at 34–66%, and three at 67–100%. They stay dark at 0% or if the reading fails. The display refreshes this roughly every 30 seconds.

## Current Radio Status (2026-11-01)

Local music, upload, deletion and title synchronization remain available. Hold OK and open 网络设置 to scan and select Wi-Fi from the hotspot page. Set 模式 to 网络电台 and save to connect; select 本地播放器 to return. Radio covers 89 sampled MP3 streams across 19 mainland cities. The original EVA frame is shared: city and station index replace the playback-time caption, the central instrument displays the Chinese station name, the track panel shows live status, and the bottom retains four controls. Double-click OK to select a city and station; hold DOWN inside the list to go back. UP/DOWN tunes, OK plays/pauses, hold UP switches source, and hold DOWN opens the hotspot. Genre, a selection hint and a live-state indicator add detail; connecting and buffering do not highlight playback. Hold OK opens settings. City selection and settings share the header, battery frame and bordered panels. Instructions are under 操作说明 in settings. Chinese subsets are generated locally from a builder-supplied CJK font. See [radio development](docs/EVA_RADIO.md) for setup, city counts and acceptance details.

The current application is 1,999,984 bytes (0x1e8470), with 97,168 bytes free in its 2 MiB partition and 112,356 bytes of static DRAM remaining. November 1 application-only flash and hash verification passed. Serial evidence covers mode switching, hotspot entry/exit, scanning, one upload, settings save, local 12 kHz and radio 44.1 kHz output. Two stations returned HTTP 200; one approximately minute-long window had no reconnect. This does not establish audible continuity, the new screen layout, all stations or long-term stability. Full host checks: 32 Python passed/1 skipped, 9 standalone C programs and 3 JavaScript scripts passed. See [current device evidence](docs/HARDWARE_TEST_20261101.md) and the [documentation index](docs/README.md).

## Build

On this Linux workstation, the installed isolated ESP-IDF/toolchain environment is accessible through `.venv/bin/python tools/idf_linux.py --check` and `.venv/bin/python tools/idf_linux.py build`. This wrapper checks paths and forwards exit codes; it does not install dependencies or generate missing media.

Use ESP-IDF 5.5.x. Supply your own font with Chinese/Japanese glyph coverage and a legally usable startup sound. Install Python Pillow and fonttools in `.venv` and Node.js/npm, then generate the local font assets before building; see [asset preparation](docs/ASSET_PREPARATION.md). These generated files are excluded from Git and must be recreated after cloning:

```powershell
py -m venv .venv
& '.\.venv\Scripts\python.exe' -m pip install pillow fonttools
.\tools\prepare_local_font.ps1 -FontFile 'C:\path\to\your-font.otf' -CjkFontFile 'C:\path\to\NotoSansSC.ttf'
.\tools\prepare_startup_audio.ps1 -InputFile 'C:\path\to\startup.mp3'
idf.py build
```

For image layout, checksums, and the distinction between current and historical packages, see [release guidance](docs/RELEASING.md).

Do not publish startup audio, user music, or a firmware image containing protected material without the required rights. See [NOTICE.md](NOTICE.md) and [docs/ASSET_PREPARATION.md](docs/ASSET_PREPARATION.md).

### Current Local Full Image (2026-11-01)

Local delivery: release/FoloToy-EVA-1.2.0-dev-full-8MB-20261101.bin, 8,388,608 bytes, flash at 0x0. It contains the bootloader, partition table and the same application verified on the device. NVS, PHY and music regions are blank; no user credentials or songs are copied. The music partition is initialized by the existing first-boot path. Flashing this complete image replaces existing songs and settings; it is not an application-only update. The complete image passed segment/blank-region verification but was not fully flashed again. The image and checksum/report remain Git-ignored. See [release instructions](docs/RELEASING.md).

### Historical October 1 Image (No Radio)

The recorded local complete image was `release/FoloToy-EVA-player-3songs-fixed-20261001.bin` (8 MiB, flash at `0x0`, replaces existing songs/settings). It includes the bootloader, partition table, application, and three deletable presets: `残酷な天使のテーゼ（TV版）`, `Beautiful World`, and `One Last Kiss`. The similarly named `.music.bin` is only the music partition; the image without `fixed` predates the three fixes. For that historical build, the complete `fixed` BIN was the full-flash input. These generated files are not included in a source checkout.

ESP-IDF 5.5.4 built app `0x19a2d0` with 417,072 bytes (20%) free in its partition; the preset music volume has 1,978,368 bytes free. Host regression tests and final package checks passed. On 2026-10-01 the complete image was written to COM14 (ESP32-C3 revision v1.1, 8 MB Flash), its data hash verified, and a hardware reset issued. This confirms flashing, not playback or UI acceptance. Playback, multiple-phone management, physical power-loss behavior, and runtime heap usage still need device validation. Preparation and packaging commands are in [asset preparation](docs/ASSET_PREPARATION.md).

## Layout

| Path | Purpose |
| --- | --- |
| `components/bsp/` | Display, buttons, audio and other board support |
| `main/` | Local player, MP3 radio, shared EVA UI, settings, catalog and Wi-Fi upload/setup |
| `assets/audio/` | Local generated startup sound, excluded from Git |
| `assets/preset_music/` | Optional generated FAM1 songs and ETT1 title masks, excluded from Git |
| `tools/prepare_music.py`, `tools/package_factory.py` | Prepare deletable presets and package a complete local image |
| `tools/prepare_local_font.ps1` | Generates untracked font glyphs, UI masks, and upload page |
| `assets/promo/` | Three 3:4 promotional images based on user-provided device photos; generated artwork, not literal hardware screenshots |
| `partitions.csv` | 2 MB app and approximately 5.9 MB music partition |
| `tests/` | Host state, radio stream/catalog/setup, settings, font, HTTP and packaging tests |

See [ARCHITECTURE.md](ARCHITECTURE.md) for design details. September builds were validated on COM14 for startup sound, uploaded songs, titles, persistence, repeated hotspot access, volume controls, and prompt NERV splash. A three-song scan then took about 13 seconds under the splash. Those observations do not establish acceptance of the October build. Raw logs and backups stay outside Git; current evidence is in [November 1 device checks](docs/HARDWARE_TEST_20261101.md); [October 30](docs/HARDWARE_TEST_20261030.md) is historical.
