# Local Audio Assets

`startup.adpcm` is a locally generated 8 kHz mono EVA1/IMA ADPCM startup sound embedded in the firmware. Generate it with `tools/prepare_startup_audio.ps1` from a source recording you may use. The generated file and source recording are excluded from Git and should not be published without redistribution rights.

User songs are uploaded through the device hotspot and stored in its Flash music partition. The older `track0.adpcm`, `track1.adpcm`, and `track2.adpcm` files are no longer firmware build inputs.
