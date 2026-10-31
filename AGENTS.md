# Repository Guidelines

## Project Structure & Module Organization

This repository is an ESP-IDF application project for the ESP32-C3-based FoloToy AI Passport. The current app boots directly into an EVA-style local music player with integrated internet radio and Chinese settings; the original hardware demo files remain as BSP references.

- `components/bsp/include/`: public BSP APIs and the hardware pin/configuration source of truth (`bsp_pins.h`).
- `components/bsp/src/`: display, button, audio, battery, and shared-I2C implementations.
- `main/`: EVA player/shared UI, MP3 radio and city catalog, Chinese settings, dynamic FAT music catalog, open Wi-Fi upload/setup pages and codec, generated UI assets, and legacy `demo_*.c` hardware references.
- `tests/`: host-side tests for player state, FAM1/ADPCM, catalog recovery, HTTP revisions, browser requests, packaging, clock, title layout, generated assets, radio streams/catalog/setup, and settings.
- `tools/`: local font/audio preparation, radio source sampling, Linux IDF entrypoint, and optional deletable-preset/full-image packaging; generated assets and `release/` remain Git-ignored.
- `docs/`: hardware, architecture, asset, release, and contribution documentation.
- `sdkconfig.defaults`: reproducible target, console, LVGL, and memory defaults.
- `README.md`: current application behavior, build instructions, asset policy, and validation notes.

Keep reusable hardware logic in `components/bsp`; keep player UI and playback behavior in `main`. Font-derived application assets are generated locally from a builder-supplied font and are excluded from Git; see `docs/ASSET_PREPARATION.md`.

## Build, Test, and Development Commands

Use ESP-IDF 5.5.x. On a fresh checkout, first prepare a local startup clip and font assets using `docs/ASSET_PREPARATION.md`:

```bash
# First activate ESP-IDF 5.5.x for your local shell.
idf.py set-target esp32c3     # Configure a fresh checkout
idf.py build                  # Compile firmware and validate dependencies
idf.py flash monitor          # Flash the connected board and open logs
idf.py fullclean              # Remove generated build state when configuration is stale
```

Host-side Python, C, and JavaScript tests exist under `tests/`. Treat a clean `idf.py build` and relevant host tests as the minimum automated check. For device changes, use the acceptance items in `docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md` and distinguish earlier device checks from the current build.

## Coding Style & Naming Conventions

Write C using four-space indentation and K&R-style braces, following nearby files. Use `snake_case` for functions and locals, `BSP_*` for public hardware constants, and `s_` for file-local state. Keep BSP APIs prefixed with `bsp_`; keep player APIs prefixed with `eva_`; legacy demo entry points use `demo_<feature>_<action>`. Prefer `static` for internal symbols. Preserve comments documenting hardware-specific register values and memory constraints.

## Testing Guidelines

Before submitting, build from the repository root and inspect warnings. Run relevant host tests. On hardware, verify startup, display, buttons, audio playback, and the affected BSP behavior. For pin, display-rotation, codec-clock, ADC, partition, asset, or DMA changes, explicitly record the observed hardware result in the PR. Do not increase LVGL buffers, generated assets, embedded audio, or audio allocations without checking ESP32-C3 Flash and internal RAM usage; the board has no PSRAM.

## Commit & Pull Request Guidelines

History follows Conventional Commit-style subjects such as `feat(player): ...`, `feat(bsp): ...`, `fix(player): ...`, `fix(bsp): ...`, and `docs: ...`. Keep commits focused by subsystem. Pull requests should explain the hardware/revision tested, summarize behavior changes, list build and on-device results, and include photos or screenshots for display changes. Link related issues and call out wiring, pin-map, asset, licensing, Flash, or compatibility impacts.

## Current Navigation and Delivery

As of 2026-11-01, long UP switches local/radio, long DOWN opens or leaves the hotspot, and long OK opens/saves settings. Double OK on radio opens city/station selection; long DOWN inside lists returns one level. Volume is edited in settings alongside brightness and screen-off; version is read-only. The navigation harness extracts production functions and runs through tests/test_eva_navigation.py.

Start documentation lookup at docs/README.md. Keep dated hardware records separate; current evidence is docs/HARDWARE_TEST_20261101.md. Clean complete images are local 8 MiB artifacts with blank NVS/music; never substitute a device dump or commit release/ outputs. Preparing a complete image does not authorize flashing it or clearing device data.
