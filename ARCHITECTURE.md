# Architecture

The BSP under `components/bsp` owns hardware drivers. `main` owns the EVA player, local music storage, transfer service, and UI. The BSP does not depend on application code.

## Startup and playback

`app_main` initializes I2C, display, buttons, battery, and NVS, then shows the NERV splash and starts a local EVA1 ADPCM clip. The FAT music partition is mounted and its catalog validated while that splash is visible. The splash stays for at least three seconds and until both the clip and catalog scan finish. The player starts stopped, with a dynamic catalog that may be empty.

The BSP pulls the backlight pin low at the start of `app_main`. The app keeps it off during early hardware initialization, forces the first NERV frame through LVGL, allows 40 ms for the final SPI transfer, then enables the backlight and starts the audio clip. This avoids showing uninitialized panel memory during startup while the longer catalog scan runs under the visible splash.

The audio worker reads one validated FAM1 block at a time from Flash, decodes it into a 512-sample PCM buffer, and writes it through the BSP codec. A track is validated for structure and CRC when uploaded and again when scanned at boot. The worker releases I2S and codec resources while paused so the open Wi-Fi access point can use internal RAM. Songs play at 8 or 12 kHz mono. At end of track, the UI holds for about two seconds before starting the next track.

Long `UP` opens a separate EVA volume screen. Short `UP` and `DOWN` change a 0–100% level in 5% steps. The audio worker applies changes to the codec while playing; the setting is saved to NVS when `OK` closes the screen and restored for playback and the startup clip on the next boot.

The CW2017 fuel gauge supplies a 0–100% state of charge. The player selects one, two, or three generated “INTERNAL” badge variants at 1–33%, 34–66%, or 67–100%; 0% and invalid readings select the unlit variant. It polls every 30 seconds.

The 8 MB Flash table allocates 2 MB to the application and the remaining approximately 5.9 MB to the FAT music partition. The startup clip is embedded in the application; user songs and per-song title masks live in FAT. The title image is rendered by the browser and uploaded as an A8 mask so the device does not need a CJK font. The display keeps two image objects for its marquee.

Fixed labels and buttons are pre-rendered from a font supplied locally by the builder. Two small ASCII subsets provide the runtime LVGL labels. The compressed browser page embeds a compact WOFF subset of the same font and loads it automatically for page text and title rendering; characters outside that subset use a browser fallback and are reported on title sync. The source font and all generated glyph/page files are excluded from Git; builders regenerate them with `tools/prepare_local_font.ps1`. The segmented clock is drawn directly into its reusable A8 canvas.

## Transfer flow

While stopped or paused, long `DOWN` enters transfer mode. `eva_wifi` starts an open AP for up to two clients and serves the compressed EVA-style page at `192.168.4.1`. The browser decodes and resamples the user's local audio, encodes FAM1/IMA ADPCM, then posts the file in chunks. The server checks length and free space, writes a temporary file, validates it, and commits it to the catalog. The page can list and delete songs and upload a title mask. The page layout follows the supplied EVA/NERV reference image, with responsive mobile layout. Long `DOWN` stops the AP and refreshes the on-device catalog. Exit is refused during an upload.

The device transfer screen reuses the player panel geometry and a generated Chinese A8 title mask. Its time and track regions show the SSID and HTTP address; the bottom panel shows the exit control.

## Key modules

| Module | Responsibility |
| --- | --- |
| `main/eva_player.c`, `eva_player_model.c` | LVGL screens, buttons, playback state and audio worker |
| `main/eva_music_store.c`, `audio_catalog.c` | FAT mount, free space, validated song files and catalog |
| `main/eva_wifi.c`, `web_ui.html`, `audio_adpcm.js` | Open AP, HTTP API, browser upload and encoding |
| `main/fam1_format.c`, `eva_adpcm.c` | Uploaded song decoder and embedded startup decoder |
| `main/eva_title.c` | Browser-rendered title mask loading |
| `components/bsp/` | Display, audio codec, buttons and board pins |

The song format, catalog layout and browser encoder follow the public FoloToy Gallery implementation, with project-specific integration and UI. See [FoloToy Gallery](https://github.com/PhoenixZHC/folotoy_gallery).

## Verification boundary

Host tests and an ESP-IDF build check logic and compilation. An earlier firmware build passed on-device startup, upload/playback, title persistence, repeated hotspot access, volume and battery display checks. The latest `1.2.0-dev` Matisse font and web layout build has only boot-log and browser-preview checks after flashing; its on-device visual and upload acceptance remain open. Flashing this table changes the old partition layout; back up data that must be preserved first.
