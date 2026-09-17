import struct
from pathlib import Path
import pytest


ROOT = Path(__file__).resolve().parents[1]
AUDIO_DIR = ROOT / "assets" / "audio"
pytestmark = pytest.mark.skipif(
    not (AUDIO_DIR / "startup.adpcm").is_file(),
    reason="local startup audio is required for asset tests",
)


def read_header(path):
    data = path.read_bytes()[:16]
    magic, sample_rate, predictor, index, reserved, flags, sample_count = struct.unpack(
        "<4sHhBBHI", data
    )
    return magic, sample_rate, predictor, index, reserved, flags, sample_count


def test_startup_audio_is_present_and_small():
    path = AUDIO_DIR / "startup.adpcm"
    assert path.exists(), path
    assert 16 < path.stat().st_size < 200_000


def test_startup_audio_uses_expected_format():
    magic, sample_rate, predictor, adpcm_index, reserved, flags, sample_count = read_header(
        AUDIO_DIR / "startup.adpcm"
    )
    assert magic == b"EVA1"
    assert sample_rate == 8000
    assert -32768 <= predictor <= 32767
    assert 0 <= adpcm_index <= 88
    assert reserved == 0
    assert flags == 0
    assert sample_count > 0
