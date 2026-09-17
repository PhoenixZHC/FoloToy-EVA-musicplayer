# Contributing

This project targets a small ESP32-C3 device with 8 MB Flash and no PSRAM. Keep changes small, measurable, and easy to test.

## Before Changing Code

1. Read [README.md](README.md), [ARCHITECTURE.md](ARCHITECTURE.md), and [docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md](docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md).
2. Check the current working tree and preserve existing user changes.
3. Decide whether the change belongs in `components/bsp` or `main`.
4. Do not copy pin numbers, I2C addresses, display sizes, or hardware constants into new files. Use `bsp_pins.h`.

## Code Rules

- Keep board-level reusable logic in `components/bsp`.
- Keep player-specific UI, state, playback, Wi-Fi upload, and catalog behavior in `main`.
- Keep pure state logic testable without ESP-IDF or LVGL when practical.
- Do not block inside button callbacks or LVGL timer callbacks.
- Hold the LVGL lock when non-LVGL tasks touch LVGL objects.
- After editing `web_ui.html`, `web_style.css`, or `audio_adpcm.js`, regenerate the ignored `web_ui.h` using `tools/prepare_local_font.ps1` and check browser behavior.
- Do not increase image, font, audio, task stack, DMA, or LVGL buffers without checking Flash and internal RAM.

## Asset Rules

Do not commit or publish copyrighted music or merged firmware images that contain protected assets unless you have rights to redistribute them. The source font, generated text/image glyphs, LVGL font subsets, and embedded WOFF page header are excluded from Git. Builders supply their own font and regenerate them locally.

Acceptable public files are:

- conversion scripts
- source code
- small self-made placeholders
- documentation explaining how users can prepare their own legal assets

See [docs/ASSET_PREPARATION.md](docs/ASSET_PREPARATION.md).

See [docs/GIT_RULES.md](docs/GIT_RULES.md) for the exact list of files that should and should not be committed.

## Required Checks

Before opening a pull request, generate local assets as described in [asset preparation](docs/ASSET_PREPARATION.md), then run the relevant host tests and an ESP-IDF build. Asset-dependent Python tests are skipped when the corresponding local font or startup sound is absent.

```powershell
$env:PYTHONPATH = "tools"
py -m pytest tests
node tests/test_audio_adpcm.js
idf.py build
```

For UI, button, audio, display, partition, or memory changes, also test on real hardware and record what was observed. Build results and an older hardware check do not establish that the newest font and upload-page revision has passed on-device acceptance.

## Pull Request Notes

Include:

- what changed
- which board was tested
- build result
- host test result
- hardware result, if relevant
- any remaining unverified hardware item

Keep asset licensing clear. Do not attach protected media or firmware images that embed protected media.
