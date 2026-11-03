# FoloToy EVA Music Player

[简体中文](README.zh_CN.md) | English

Music player firmware for the FoloToy AI Passport (ESP32-C3, 8 MB Flash), with an integrated internet radio. Current source version: `1.2.1`. The NERV splash plays a locally prepared startup sound. Local songs live in a dedicated Flash partition and can be uploaded from a phone through the device's open Wi-Fi hotspot.

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

Hold OK to open the Chinese settings menu. UP/DOWN selects a row, OK edits/confirms, and holding OK or choosing 保存并返回 saves and applies. 模式 selects local music or radio; 亮度 is 10–100%; 音量 is 0–100%; 版本 shows the actual firmware version; 自动熄屏 is never/15/30/60/120/300 seconds; 网络设置 opens the setup hotspot. On timeout the display shows the screensaver or disables the backlight according to 屏保开关, and the first key wakes without triggering another action. Settings and source mode persist across restarts. At song end, playback waits about two seconds before advancing.

Automatic screen-off can instead show the startup NERV logo as a screensaver while audio continues. In settings, 屏保开关 enables/disables it and 屏保亮度 sets its independent brightness from 10–100% in 10% steps (default: enabled, 20%). The existing screen-off timeout triggers it; 常亮 disables the idle timeout. Disabling the screensaver restores backlight-off behavior. The first key only wakes the original page at normal brightness. Both settings persist, and older settings are loaded without losing mode, brightness or timeout. The startup splash itself still uses normal display brightness. On November 3 the user confirmed the logo, dimming and first-key wake on the earlier screensaver build. Full brightness range, toggle-off and restart persistence remain pending; see docs/HARDWARE_TEST_20261103.md for the later repository-config build and its separate test scope.

The red marks beside “INTERNAL” indicate CW2017 battery charge: one at 1–33%, two at 34–66%, and three at 67–100%. They stay dark at 0% or if the reading fails. The display refreshes this roughly every 30 seconds.

## Current Radio Status (2026-11-03)

Local music, upload, deletion and title synchronization remain available. Hold OK and open 网络设置 to scan and select Wi-Fi from the hotspot page. Set 模式 to 网络电台 and save to connect; select 本地播放器 to return. Radio covers 89 sampled MP3 streams across 19 mainland cities. The original EVA frame is shared: city and station index replace the playback-time caption, the central instrument displays the Chinese station name, the track panel shows live status, and the bottom retains four controls. Double-click OK to select a city and station; hold DOWN inside the list to go back. UP/DOWN tunes, OK plays/pauses, hold UP switches source, and hold DOWN opens the hotspot. Genre, a selection hint and a live-state indicator add detail; connecting and buffering do not highlight playback. Hold OK opens settings. City selection and settings share the header, battery frame and bordered panels. Instructions are under 操作说明 in settings. Chinese subsets are generated locally from a builder-supplied CJK font. See [radio development](docs/EVA_RADIO.md) for setup, city counts and acceptance details.

Formal version **1.2.1** has no development suffix. ESP-IDF 5.5.4 build/size passed with repository defaults. Application: 1,999,696 bytes; app partition free: 97,456 bytes; static DRAM free: 112,422 bytes. Host regression: 47 passed, 0 failed. Earlier 1.2.0-dev with the same playback fixes passed application-only flash/hash verification and reached 99.4% steady PCM/wall time. The user reported essentially no stutter and cancelled the 30-minute run. Remaining 91–204 ms feed gaps and rebuffer events prevent a long-term zero-stutter guarantee. Formal 1.2.1 was rebuilt and packaged without another flash. See [November 3 evidence](docs/HARDWARE_TEST_20261103.md).

## Build

On this Linux workstation, the installed isolated ESP-IDF/toolchain environment is accessible through `.venv/bin/python tools/idf_linux.py --check` and `.venv/bin/python tools/idf_linux.py build`. This wrapper checks paths and forwards exit codes; it does not install dependencies or generate missing media.

The Linux entrypoint uses `build_linux_repo/` by default and keeps its effective `sdkconfig` inside that build directory, with target and defaults taken from the repository's `sdkconfig.defaults`. It never reuses the historical root `sdkconfig`. An explicit `-B` selects another isolated directory. Every build, including direct `idf.py build`, runs `tools/check_sdkconfig.py` and fails if any declared repository default differs; do not bypass this check or flash historical build outputs. The current verified application is `build_linux_repo/FoloToy-AI-Passport.bin`; see [November 3 device evidence](docs/HARDWARE_TEST_20261103.md) for its checksum and remaining radio continuity limitations.

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

### Current Local Full Image (2026-11-03)

Formal full image: `release/FoloToy-EVA-1.2.1-full-8MB-20261103.bin`, 8,388,608 bytes, ESP32-C3 / 8MB / DIO / 80MHz, flash at `0x0`. Bootloader, partition table and 1.2.1 application come from the same build_linux_repo/ build. Component, version and blank-region checks passed. NVS, PHY and music are 0xFF, without user credentials or songs; embedded startup audio and UI fonts remain. **Full flashing replaces existing songs and settings.** This image was not flashed. The BIN and same-name SHA256/JSON companions stay Git-ignored. SHA256: `0974a5164f1dd115d597bb728bc9a7b3e542865fdd684656a0d49d87db0c4ac9`.

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

See [ARCHITECTURE.md](ARCHITECTURE.md) for design details. September builds were validated on COM14 for startup sound, uploaded songs, titles, persistence, repeated hotspot access, volume controls, and prompt NERV splash. A three-song scan then took about 13 seconds under the splash. Those observations do not establish acceptance of the October build. Raw logs and backups stay outside Git; current evidence is in [November 3 device checks](docs/HARDWARE_TEST_20261103.md); [October 30](docs/HARDWARE_TEST_20261030.md) is historical.
