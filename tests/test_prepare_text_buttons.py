from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from prepare_text_buttons import alpha_bytes, build_source, render_text


def test_button_assets_use_transparent_a8_masks():
    image = render_text("PLAY", 18)
    data = alpha_bytes(image)

    assert len(data) == image.width * image.height
    assert 0 in data
    assert max(data) == 255

    source, _ = build_source([("eva_text_btn_play", "PLAY", 18)])
    assert "LV_COLOR_FORMAT_A8" in source
    assert f".header.stride = {image.width}" in source
