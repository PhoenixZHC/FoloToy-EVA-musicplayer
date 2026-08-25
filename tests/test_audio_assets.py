import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
AUDIO_DIR = ROOT / "assets" / "audio"


def read_header(path):
    data = path.read_bytes()[:16]
    magic, sample_rate, predictor, index, reserved, flags, sample_count = struct.unpack(
        "<4sHhBBHI", data
    )
    return magic, sample_rate, predictor, index, reserved, flags, sample_count


def test_adpcm_assets_are_present_and_fit_flash_budget():
    paths = [AUDIO_DIR / f"track{i}.adpcm" for i in range(3)]

    for path in paths:
        assert path.exists(), path
        assert path.stat().st_size > 100_000

    total = sum(path.stat().st_size for path in paths)
    assert total < 3_600_000


def test_adpcm_assets_use_expected_format():
    for index in range(3):
        magic, sample_rate, predictor, adpcm_index, reserved, flags, sample_count = read_header(
            AUDIO_DIR / f"track{index}.adpcm"
        )
        assert magic == b"EVA1"
        assert sample_rate == 8000
        assert -32768 <= predictor <= 32767
        assert 0 <= adpcm_index <= 88
        assert reserved == 0
        assert flags == 0
        assert sample_count > 8000
