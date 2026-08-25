from pathlib import Path
import sys

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from prepare_nerv_logo import build_logo_image, rgb565_bytes


def test_build_logo_image_replaces_white_with_black_and_keeps_red():
    src = Image.new("RGB", (20, 20), "white")
    for x in range(5, 15):
        for y in range(6, 14):
            src.putpixel((x, y), (230, 25, 20))

    logo = build_logo_image(src, 12, 12)

    assert logo.size == (12, 12)
    pixels = list(logo.getdata())
    assert (0, 0, 0) in pixels
    assert not any(r > 240 and g > 240 and b > 240 for r, g, b in pixels)
    assert any(r > 180 and g < 80 and b < 80 for r, g, b in pixels)


def test_rgb565_bytes_are_little_endian_rgb565_for_lvgl():
    img = Image.new("RGB", (1, 1), (255, 0, 0))
    assert rgb565_bytes(img) == [0x00, 0xF8]
