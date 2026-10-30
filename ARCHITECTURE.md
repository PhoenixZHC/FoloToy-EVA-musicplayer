# Architecture

The BSP under `components/bsp` owns hardware drivers. `main` owns the EVA player, local music storage, internet radio, settings, transfer/setup service, and UI. The BSP does not depend on application code.

## Startup and playback

`app_main` initializes I2C, display, buttons, battery, and NVS, then shows the NERV splash and starts a local EVA1 ADPCM clip. The FAT music partition is mounted and its catalog validated while that splash is visible. The splash stays for at least three seconds and until both the clip and catalog scan finish. The local player starts stopped with a dynamic catalog that may be empty; a persisted radio mode starts only after the splash and catalog scan finish.

The BSP pulls the backlight pin low at the start of `app_main`. The app keeps it off during early hardware initialization, forces the first NERV frame through LVGL, allows 40 ms for the final SPI transfer, then enables the backlight and starts the audio clip. This avoids showing uninitialized panel memory during startup while the longer catalog scan runs under the visible splash.

The audio worker reads one validated FAM1 block at a time from Flash, decodes it into a 512-sample PCM buffer, and writes it through the BSP codec. A track is validated for structure and CRC when uploaded and again when scanned at boot. The worker releases I2S and codec resources while paused so the open Wi-Fi access point can use internal RAM. Songs play at 8 or 12 kHz mono. At end of track, the UI holds for about two seconds before starting the next track.

Long `UP` opens a separate EVA volume screen. Short `UP` and `DOWN` change a 0–100% level in 5% steps. The BSP installs an ES8311 volume curve with points (0%, -50 dB), (70%, -15 dB), and (100%, +6 dB); 0% explicitly mutes output. This preserves the former levels through 70% and raises only the upper range, without scaling/clipping PCM or changing assumed PA hardware parameters. The audio worker applies changes to the codec while playing; the setting is saved to NVS when `OK` closes the screen and restored for playback and the startup clip on the next boot.

The CW2017 fuel gauge supplies a 0–100% state of charge. The player selects one, two, or three generated “INTERNAL” badge variants at 1–33%, 34–66%, or 67–100%; 0% and invalid readings select the unlit variant. It polls every 30 seconds.

The 8 MB Flash table allocates 2 MB to the application and the remaining approximately 5.9 MB to the FAT music partition. The startup clip is embedded in the application; user songs and per-song title masks live in FAT. The title image is rendered by the browser and uploaded as an A8 mask so local song titles do not require a device font containing every possible character; radio and settings have a separate small CJK subset. The display keeps two image objects for its marquee.

The optional factory packaging path prepares the same FAM1/ETT1 files using `tools/prepare_music.py`, then `tools/package_factory.py` creates an ESP-IDF wear-levelled FAT image at the existing music partition offset and merges the complete 8 MiB image. The mount code accepts this valid volume without formatting it, even on first boot with blank NVS. Presets have no special runtime flag or embedded backup: the normal catalog, delete and upload paths handle them. Emptying the library remains persistent; only flashing the complete factory image restores its initial files. The application partition and playback buffer sizes are unchanged.

Songs and their title masks use stable FAT slots (`aNNN.fam` / `tNNN.bin`). The visible playlist is sorted by occupied slot, with consecutive display indices. Deleting a song removes its optional title first and then its audio, without renaming other songs. A restart accepts gaps, including gaps left by the earlier delete algorithm. An interruption between the two removals leaves a playable song without its title mask; the existing title-sync operation restores the mask. Actual FAT corruption remains an error. Uploads reuse the first free slot, remove any orphan title in that slot, insert the new song at its sorted position, and return that exact visible index. No permanent migration or new on-disk metadata is required.

Fixed labels and buttons are pre-rendered from a font supplied locally by the builder. Two small ASCII subsets provide the runtime LVGL labels. The compressed browser page embeds a compact WOFF subset of the same font and loads it automatically for page text and title rendering; characters outside that subset use a browser fallback and are reported on title sync. The source font and all generated glyph/page files are excluded from Git; builders regenerate them with `tools/prepare_local_font.ps1`. The segmented clock is drawn directly into its reusable A8 canvas.

## Transfer flow

While stopped or paused, long `DOWN` enters transfer mode. `eva_wifi` starts an open AP for up to two clients and serves the compressed EVA-style page at `192.168.4.1`. The browser decodes and resamples the user's local audio, encodes FAM1/IMA ADPCM, then posts the file in chunks. The server checks length and free space, writes a temporary file, validates it, and commits it to the catalog. The page can list and delete songs and upload a title mask. The page layout follows the supplied EVA/NERV reference image, with responsive mobile layout. Long `DOWN` stops the AP and refreshes the on-device catalog. Exit is refused during an upload.

The device transfer screen reuses the player panel geometry and a generated Chinese A8 title mask. Its time and track regions show the SSID and HTTP address; the bottom panel shows the exit control.

Catalog responses and upload results include an opaque revision composed of a random AP-session token and a catalog generation. Add/delete operations advance the generation. Delete and title-sync requests must supply the revision associated with their displayed index; stale or missing revisions receive HTTP 409 before file mutation. Each page row retains its own revision. On conflict the browser refreshes and asks the user to select again, without retrying the mutation. This also protects the separate title request following an upload and pages left open across AP restarts.

At end of track, navigation-key press/long-press events retain the two-second auto-advance deadline. Only a confirmed navigation click cancels it; the volume screen therefore does not cancel the next song.

## Internet radio

Device settings use the pure eva_settings model and eva_settings_store NVS adapter. The LVGL overlay holds a draft configuration; brightness previews immediately, while source changes are deferred until saved. Defaults retain 100% brightness and no automatic screen-off. The timeout only turns off the backlight and consumes the first wake key. Persisted source mode is restored after the splash and catalog scan complete.

The setup handlers and embedded page now live in main/eva_radio_setup.c and main/eva_radio_setup.html. GET /radio/scan temporarily switches the transfer driver from AP to APSTA without connecting an uplink, scans, releases the scan list, and restores AP on success or error. The page sorts the strongest 20 access points by signal, deduplicates names, and asks only for the selected network's password. Unsupported authentication remains visible but disabled. These scan operations do not launch the radio worker.

Radio is a source mode inside the existing player, selected through the MODE row of the settings menu. Holding OK opens settings from local, radio or transfer screens. Local FAM1 files and storage are unchanged. A deferred source request waits for radio cleanup or local I2S/codec release before starting the destination; NETWORK stops playback and opens the existing setup hotspot. Settings edits do not stop current playback until an actual source/network change is applied. SoftAP transfer and STA radio playback remain mutually exclusive.

`eva_radio_catalog` stores 89 MP3 entries across 19 mainland cities, with source dates. Existing station IDs are retained for NVS compatibility; cities and each city list stay below the 32-entry UI limit. The host checker in tools/check_radio_sources.py accepts only HTTP audio responses with at least eight complete consecutive compatible MP3 frames; this sampling does not establish long-term availability or station identity. `eva_radio_config` persists router credentials and last selected station in a separate NVS namespace. The existing upload HTTP server exposes `/radio/setup` and a JSON-only `/radio/network` POST; credentials are never returned or logged. The hotspot remains open, so setup must occur in a trusted environment.

`eva_radio_player` owns one STA/HTTP/decoder task. It selects the strongest matching AP after scanning all channels, bounds redirects, retries connections and streams with exponential backoff capped at 30 seconds, strips ICY metadata, validates MP3 frame boundaries, decodes with ESP Audio Codec 2.6.2, downmixes stereo to mono, and uses existing BSP audio APIs. Input/PCM buffers are 4096/4608 bytes and task stack is 6144 bytes. Heap, largest internal block and stack watermark are logged at playback and release. All curated streams use HTTP to avoid certificate-clock/TLS heap requirements; stream transport is unencrypted. HLS and stream-title display are not implemented. Radio, city selection and settings use generated 16/20 px Chinese font subsets. See [radio development](docs/EVA_RADIO.md) for current checks and device acceptance gates.

## Key modules

The Chinese radio screen and city/station picker live in main/eva_radio_view.c. Local, radio, city selection and settings share main/eva_player_frame.c for their EVA header and bordered panels. Radio replaces the playback-time caption with city and station index, the central instrument with the station name, and the track panel with live status; the bottom retains four controls. Instructions live in a settings help view without changing persisted settings fields. main/eva_player.c owns button routing and mode transitions; clearing the radio-exit flag after cleanup preserves local-mode settings access. tools/prepare_radio_fonts.py validates the supplied CJK font and generates ignored 16/20 px subsets.

Radio playback uses bsp_audio_init_playback(), which creates only the I2S TX channel and DAC output; bsp_audio_init() retains full duplex for local/legacy callers. The LVGL pool is now 48 KiB (Chinese UI peak previously below 20 KiB), reserving memory for a TCP receive window trial of 11,520 bytes with a 10-entry receive mailbox. SACK is enabled. The first or resumed read prebuffers 3584 bytes in the existing input buffer before decoding. The user confirmed the final EVA layout, about one minute of continuous audio and improved loudness; longer and cross-network stability remain unverified. Previously, measured startup/radio UI peak was 13,560 bytes, while the former 96 KiB pool caused MP3 initialization to return ESP_AUDIO_ERR_MEM_LACK. Network read EAGAIN retains pending frame data and waits up to 10 seconds without new bytes before reconnecting. Other read failures and EOF reconnect immediately; allocation failure ends the worker instead of retrying indefinitely. Playback heap/stack measurements are emitted every 30 seconds.

| Module | Responsibility |
| --- | --- |
| main/eva_player_frame.[ch] | Shared EVA panels, title and battery for local/radio/settings |
| main/eva_radio_* | City catalog, state, view, stream validation, setup, NVS and MP3 worker |
| main/eva_settings* | Settings model and NVS persistence |
| `main/eva_player.c`, `eva_player_model.c` | LVGL screens, buttons, playback state and audio worker |
| `main/eva_music_store.c`, `audio_catalog.c` | FAT mount, free space, validated song files and catalog |
| `main/eva_wifi.c`, `web_ui.html`, `audio_adpcm.js` | Open AP, HTTP API, browser upload and encoding |
| `main/fam1_format.c`, `eva_adpcm.c` | Uploaded song decoder and embedded startup decoder |
| `main/eva_title.c` | Browser-rendered title mask loading |
| `components/bsp/` | Display, audio codec, buttons and board pins |

The song format, catalog layout and browser encoder follow the public FoloToy Gallery implementation, with project-specific integration and UI. See [FoloToy Gallery](https://github.com/PhoenixZHC/folotoy_gallery).

## Verification boundary

Host tests and an ESP-IDF build check logic and compilation. The October 30 volume/catalog build passed 30 Python tests (1 skipped), 9 standalone C programs and 3 JavaScript scripts, and its application-only flash passed hash verification. The user confirmed the final EVA layout, one minute of continuous listening, settings access after returning to local mode and improved loudness. Current app size is 2020288 bytes, with 76864 bytes left in the 2 MiB app partition and 112364 bytes of static DRAM remaining. Explicit maximum-volume distortion checks, exhaustive stations, long running/cross-network playback, brightness/timeouts and current upload/deletion/power-loss regression remain open. See [hardware evidence](docs/HARDWARE_TEST_20261030.md). Application-only updates preserve NVS and music; a complete factory image replaces songs and settings.
