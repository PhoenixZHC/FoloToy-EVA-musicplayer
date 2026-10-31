"""中文实际生成物的字形覆盖和 240px 屏幕文字宽度回归。"""
from pathlib import Path
import re
import ast
import sys
import pytest
from PIL import ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from prepare_radio_fonts import required_characters

@pytest.mark.parametrize('size', [16, 20])
def test_every_radio_and_settings_character_has_a_generated_glyph(size):
    path = ROOT / f'main/eva_font_radio_{size}.c'
    if not path.is_file():
        pytest.skip('local Chinese font assets are required')
    source = path.read_text(encoding='utf-8')
    glyphs = {int(value, 16) for value in re.findall(r'U[+]([0-9A-Fa-f]+)', source)}
    missing = set(map(ord, required_characters())) - glyphs
    assert not missing, ''.join(map(chr, sorted(missing)))

def test_chinese_menu_character_set_is_present():
    chars = set(required_characters())
    assert set('系统设置网络电台城市选择本地播放器亮度自动熄屏保存并返回音量版本双击') <= chars

def test_new_navigation_captions_fit_their_panels():
    path = ROOT / "assets/fonts/radio-source.otf"
    if not path.is_file():
        pytest.skip("local Chinese source font is required")
    font = ImageFont.truetype(str(path), 16)
    for caption in ("MP3 / 双击OK选择城市", "长按下返回 / 长按OK设置",
                    "上下选择 / OK编辑或查看"):
        assert font.getlength(caption) <= 222
    catalog = (ROOT / "main/eva_radio_catalog.c").read_text(encoding="utf-8")
    genres = re.findall(r'\{\s*\d+,\s*\d+,\s*"[^"]+",\s*"[^"]+",\s*"([^"]+)"', catalog)
    assert genres
    assert all(font.getlength(genre + " 32/32") <= 126 for genre in genres)

def test_settings_help_and_radio_caption_fit_their_panels():
    path = ROOT / "assets/fonts/radio-source.otf"
    if not path.is_file():
        pytest.skip("local Chinese source font is required")
    small = ImageFont.truetype(str(path), 16)
    large = ImageFont.truetype(str(path), 20)
    assert large.getlength("上海") <= 126
    assert small.getlength("电台 02/02") <= 126
    source = (ROOT / "main/eva_player.c").read_text(encoding="utf-8")
    match = re.search(r"lv_label_set_text\(help, (\"[^\"]*\")\);", source)
    assert match, "settings must contain an operation guide"
    guide = ast.literal_eval(match.group(1))
    lines = guide.splitlines()
    assert len(lines) >= 8
    assert len(lines) * 19 <= 180
    assert all(small.getlength(line) <= 212 for line in lines)

    catalog = (ROOT / "main/eva_radio_catalog.c").read_text(encoding="utf-8")
    stations = re.findall(r"\{\s*\d+,\s*\d+,\s*\"([^\"]+)\"", catalog)
    assert stations
    for station in stations:
        lines, current = 1, ""
        for character in station:
            if large.getlength(current + character) > 210:
                lines += 1
                current = character
            else:
                current += character
        assert lines * 24 <= 78, station
