# Architecture

[简体中文](README.zh_CN.md) | [README](README.md)

This project has two layers:

- `components/bsp`: board support code for the FoloToy AI Passport hardware.
- `main`: the EVA-style music player application.

The application may call BSP APIs, but BSP code must not depend on the player.

## Startup Flow

```text
app_main
  -> initialize shared I2C
  -> initialize display and LVGL
  -> initialize buttons
  -> initialize audio and battery services
  -> show NERV-style splash for 3 seconds
  -> create EVA player screen
```

Playback starts stopped. The audio worker is only used when playback is active.

## Main Modules

| Module | Responsibility |
| --- | --- |
| `main/main.c` | Hardware startup and application entry |
| `main/eva_player.c` | LVGL screen, input handling, playback task, UI updates |
| `main/eva_player_model.c` | Pure playback state: play, pause, previous, next, auto-next |
| `main/eva_adpcm.c` | Custom IMA ADPCM decoding for embedded audio |
| `main/eva_clock.c` | Playback time formatting and seven-segment drawing data |
| `main/eva_track_layout.c` | Track title fitting and marquee layout |
| `main/ui_pixel.c` | Small pixel/UI drawing helpers |
| `main/ui_pixel_math.c` | Host-testable color and geometry helpers |
| `main/eva_text_assets.c` | Generated text images |
| `main/eva_text_buttons.c` | Generated button label masks |
| `main/eva_logo_assets.c` | Generated local logo image asset |

The old `demo_*.c` hardware pages remain useful as BSP examples, but the current app boots directly into the player.

## Resource Strategy

The device has 8 MB Flash and no PSRAM. The player therefore avoids runtime font loading and large decoded audio buffers.

- Japanese UI text is pre-rendered into small image assets.
- Track titles are rendered as masks and scrolled inside a fixed title box.
- Music is compressed as 8 kHz mono 4-bit ADPCM and embedded into the firmware.
- The partition table gives the factory app 4 MB because the default 1 MB app slot is too small.
- LVGL object count and dynamic allocations are kept small because audio DMA and display buffers also use internal RAM.

## Input Model

The board has three buttons on one ADC ladder.

| Physical input | UI result |
| --- | --- |
| `OK` click | Toggle play/pause |
| `OK` long press while paused | Show NERV-logo standby screen |
| `OK` click on standby screen | Return to the paused player at the same position |
| `UP` press | Highlight `PREV` |
| `UP` release/click | Switch to previous track |
| `DOWN` press | Highlight `NEXT` |
| `DOWN` release/click | Switch to next track |

Button callbacks must stay lightweight. Slow work, especially audio output, belongs in a worker task.

## Playback Rules

- Boot state is stopped.
- Play resumes from the current track position.
- Pause freezes the current position.
- Standby is only entered from manual pause, not while playing or during the 2-second end-of-track hold.
- Waking from standby keeps the same track, elapsed time, and paused state.
- Manual previous/next changes track and resets the time.
- At the real ADPCM end of a track, the UI holds the final timestamp for about 2 seconds.
- After that hold, playback advances to the next track and continues.
- The last track wraps to the first track.

## Asset Boundary

The repository can publish conversion scripts, source code, and generated UI image assets. It should not publish protected songs or generated firmware images containing protected audio unless redistributable rights are confirmed.

For this reason, generated local assets may exist in a developer checkout while still being unsuitable for a public release. See [NOTICE.md](NOTICE.md) and [docs/ASSET_PREPARATION.md](docs/ASSET_PREPARATION.md).

## Verification Boundary

Host tests cover pure logic and generated asset properties. Hardware behavior still needs a real FoloToy AI Passport because display color, button thresholds, audio output, I2C behavior, and memory pressure cannot be fully proven on a PC.
