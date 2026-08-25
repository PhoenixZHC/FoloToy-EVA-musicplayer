# Contributing

This project targets a small ESP32-C3 device with 8 MB Flash and no PSRAM. Keep changes small, measurable, and easy to test.

## Before Changing Code

1. Read [README.md](README.md), [ARCHITECTURE.md](ARCHITECTURE.md), and [docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md](docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md).
2. Check the current working tree and preserve existing user changes.
3. Decide whether the change belongs in `components/bsp` or `main`.
4. Do not copy pin numbers, I2C addresses, display sizes, or hardware constants into new files. Use `bsp_pins.h`.

## Code Rules

- Keep board-level reusable logic in `components/bsp`.
- Keep player-specific UI, state, and playback behavior in `main`.
- Keep pure state logic testable without ESP-IDF or LVGL when practical.
- Do not block inside button callbacks or LVGL timer callbacks.
- Hold the LVGL lock when non-LVGL tasks touch LVGL objects.
- Do not increase image, font, audio, task stack, DMA, or LVGL buffers without checking Flash and internal RAM.

## Asset Rules

Do not commit or publish copyrighted music or merged firmware images that contain protected assets unless you have rights to redistribute them. Generated UI image assets are committed with the source because the player interface depends on them.

Acceptable public files are:

- conversion scripts
- source code
- small self-made placeholders
- documentation explaining how users can prepare their own legal assets

See [docs/ASSET_PREPARATION.md](docs/ASSET_PREPARATION.md).

See [docs/GIT_RULES.md](docs/GIT_RULES.md) for the exact list of files that should and should not be committed.

## Required Checks

Before opening a pull request, run the relevant host tests and an ESP-IDF build.

```powershell
$env:PYTHONPATH = "tools"
py -m pytest tests
idf.py build
```

For UI, button, audio, display, partition, or memory changes, also test on real hardware and record what was observed.

## Pull Request Notes

Include:

- what changed
- which board was tested
- build result
- host test result
- hardware result, if relevant
- any remaining unverified hardware item

Keep asset licensing clear. Do not attach protected media or firmware images that embed protected media.
