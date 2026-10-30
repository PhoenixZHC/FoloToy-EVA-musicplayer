# Local Audio Assets

`startup.adpcm` is a locally generated 8 kHz mono EVA1/IMA ADPCM startup sound embedded in the firmware. Generate it with `tools/prepare_startup_audio.ps1` from a source recording you may use. The generated file and source recording are excluded from Git and should not be published without redistribution rights.

User songs are uploaded through the device hotspot and stored in its Flash music partition. Optional factory presets use the same writable catalog and can be deleted normally. Generate them with `tools/prepare_music.py` into the separate Git-ignored `assets/preset_music/` directory, then package with `tools/package_factory.py`; see [Asset Preparation](../../docs/ASSET_PREPARATION.md). The older `track0.adpcm`, `track1.adpcm`, and `track2.adpcm` files are no longer firmware build inputs.

The startup clip uses the persisted player volume and the same ES8311 curve as local music/radio. The current curve retains levels through 70%, raises 100% to +6 dB and explicitly mutes at 0%; changing audio assets still requires a build-size and real-device listening check. No audio or binary assets are added by the radio feature.
