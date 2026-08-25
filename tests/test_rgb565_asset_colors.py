import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def brightest_rgb565(path, data_name):
    src = path.read_text(encoding="utf-8")
    match = re.search(
        rf"static const uint8_t\s+{re.escape(data_name)}\[\]\s*=\s*\{{(.*?)\}};",
        src,
        re.S,
    )
    assert match, data_name
    data = [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", match.group(1))]
    brightest = None
    for low, high in zip(data[0::2], data[1::2]):
        value = low | (high << 8)
        if value:
            red = ((value >> 11) & 0x1F) * 255 // 31
            green = ((value >> 5) & 0x3F) * 255 // 63
            blue = (value & 0x1F) * 255 // 31
            if brightest is None or red + green + blue > sum(brightest):
                brightest = (red, green, blue)
    if brightest is None:
        raise AssertionError(f"{data_name} has no colored pixels")
    return brightest


def test_generated_logo_pixels_are_red_when_read_as_lvgl_rgb565():
    red, green, blue = brightest_rgb565(
        ROOT / "main" / "eva_logo_assets.c",
        "eva_logo_nerv_data",
    )

    assert red > 180
    assert green < 80
    assert blue < 80
