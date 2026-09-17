from pathlib import Path
import os
import sys
import pytest

FONT_FILE = Path(os.environ.get("EVA_FONT_PATH", Path(__file__).resolve().parents[1] / "FOT-MatissePro-EB.otf"))
pytestmark = pytest.mark.skipif(
    not FONT_FILE.is_file(),
    reason="local font is required for generated-asset tests",
)

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from prepare_text_assets import build_assets


def test_track_assets_have_transparent_side_padding():
    assets = {asset.name: asset for asset in build_assets()}
    track = assets["eva_text_track_2"].image

    assert track.mode == "L"
    assert all(track.getpixel((x, y)) == 0 for x in range(4) for y in range(track.height))
    assert all(
        track.getpixel((track.width - 1 - x, y)) == 0
        for x in range(4)
        for y in range(track.height)
    )


def test_internal_panel_contains_orange_and_magenta_pixels():
    assets = {asset.name: asset for asset in build_assets()}
    panel = assets["eva_text_internal_jp"].image.convert("RGBA")
    opaque = [pixel for pixel in panel.getdata() if pixel[3] > 200]

    assert panel.size == (90, 40)
    assert any(r > 200 and 60 < g < 180 and b < 40 for r, g, b, _ in opaque)
    assert any(r > 180 and g < 40 and 60 < b < 150 for r, g, b, _ in opaque)


def test_internal_battery_bars_are_generated_in_order():
    assets = {asset.name: asset for asset in build_assets()}
    images = [assets[f"eva_text_internal_battery_{count}"].image.convert("RGBA")
              for count in range(3)]
    images.append(assets["eva_text_internal_jp"].image.convert("RGBA"))
    magenta = [sum(r > 180 and g < 40 and 60 < b < 150 and a > 200
                   for r, g, b, a in image.getdata()) for image in images]
    assert magenta[0] == 0
    assert magenta[0] < magenta[1] < magenta[2] < magenta[3]


def test_system_panel_fits_target_frame():
    assets = {asset.name: asset for asset in build_assets()}
    panel = assets["eva_text_system_jp"].image.convert("RGBA")

    assert panel.size == (90, 26)
    assert any(panel.getpixel((x, y))[3] for x in range(90) for y in range(0, 12))
    assert any(panel.getpixel((x, y))[3] for x in range(90) for y in range(14, 26))


def test_evangelion_wordmark_fits_the_green_header_panel():
    assets = {asset.name: asset for asset in build_assets()}
    wordmark = assets["eva_text_evangelion"].image

    assert wordmark.mode == "L"
    assert wordmark.width <= 126
    assert wordmark.height <= 28
    assert wordmark.getbbox() is not None
    _, top, _, bottom = wordmark.getbbox()
    assert abs(top - (wordmark.height - bottom)) <= 1
    ink_pixels = sum(1 for pixel in wordmark.getdata() if pixel >= 128)
    assert ink_pixels > 100
