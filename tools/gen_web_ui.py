"""Compress the local EVA upload page and its audio encoder for firmware embedding."""
import base64
import gzip
from io import BytesIO
import json
import os
from pathlib import Path
import re

from fontTools import subset
from fontTools.ttLib import TTFont
from PIL import Image

root = Path(__file__).resolve().parents[1]
template = (root / "main/web_ui.html").read_text(encoding="utf-8")
style = (root / "main/web_style.css").read_text(encoding="utf-8")
encoder = (root / "main/audio_adpcm.js").read_text(encoding="utf-8")
marker = "/*__AUDIO_ENCODER__*/"
if template.count(marker) != 1:
    raise SystemExit("web_ui.html must contain one encoder marker")
layout_marker = "/*__EVA_LAYOUT__*/"
if template.count(layout_marker) != 1:
    raise SystemExit("web_ui.html must contain one layout marker")
font_marker = "__EVA_FONT_WOFF__"
if style.count(font_marker) != 1:
    raise SystemExit("web_style.css must contain one EVA font marker")
font_path = Path(os.environ.get("EVA_FONT_PATH", root / "FOT-MatissePro-EB.otf"))
if not font_path.is_file():
    raise SystemExit(f"Supply your own font with EVA_FONT_PATH: {font_path}")
font = TTFont(font_path)
subsetter = subset.Subsetter()
subsetter.populate(
    text=template + style + "残酷な天使のテーゼ One Last Kiss Beautiful World",
    unicodes=range(0x20, 0x7F),
)
subsetter.subset(font)
font_codes = sorted(font.getBestCmap())
font.flavor = "woff"
font_bytes = BytesIO()
font.save(font_bytes)
font.close()

logo_source = (root / "main/eva_logo_assets.c").read_text(encoding="utf-8")
logo_match = re.search(r"static const uint8_t eva_logo_nerv_data\[\] = \{(.*?)\};", logo_source, re.S)
if not logo_match:
    raise SystemExit("eva_logo_nerv_data is missing")
logo_rgb565 = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", logo_match.group(1)))
if len(logo_rgb565) != 180 * 180 * 2:
    raise SystemExit("unexpected NERV logo size")
logo_rgba = bytearray()
for low, high in zip(logo_rgb565[::2], logo_rgb565[1::2]):
    value = low | (high << 8)
    red = ((value >> 11) & 31) * 255 // 31
    green = ((value >> 5) & 63) * 255 // 63
    blue = (value & 31) * 255 // 31
    logo_rgba.extend((red, green, blue, 255 if red > green * 1.8 and red > blue * 1.8 else 0))
logo_png = BytesIO()
Image.frombytes("RGBA", (180, 180), bytes(logo_rgba)).save(logo_png, format="PNG", optimize=True)

html = template.replace(marker, encoder).replace(layout_marker, style)
html = html.replace(font_marker, base64.b64encode(font_bytes.getvalue()).decode("ascii"))
html = html.replace("__NERV_LOGO_PNG__", base64.b64encode(logo_png.getvalue()).decode("ascii"))
html = html.replace("/*__EVA_FONT_CODES__*/", json.dumps(font_codes, separators=(",", ":")))
data = gzip.compress(html.encode("utf-8"), compresslevel=9, mtime=0)
body = ",".join(str(byte) for byte in data)
output = root / "main/web_ui.h"
output.write_text(
    "#pragma once\n#include <stdint.h>\n"
    f"static const uint8_t WEB_UI_HTML[] = {{{body}}};\n"
    f"static const unsigned int WEB_UI_HTML_LEN = {len(data)};\n",
    encoding="utf-8",
)
print(f"generated {output}: {len(data)} bytes")
