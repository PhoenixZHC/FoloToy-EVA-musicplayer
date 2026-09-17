import argparse
import os
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


SERIF_FONT = Path(os.environ.get("EVA_FONT_PATH", Path(__file__).resolve().parents[1] / "FOT-MatissePro-EB.otf"))
ORANGE = (255, 136, 0, 255)
MAGENTA = (233, 0, 77, 255)


@dataclass
class Asset:
    name: str
    image: Image.Image
    color_format: str


def font(size):
    if not SERIF_FONT.is_file():
        raise FileNotFoundError(f"Supply your own font with EVA_FONT_PATH: {SERIF_FONT}")
    return ImageFont.truetype(SERIF_FONT, size)


def text_size(text, text_font):
    bbox = ImageDraw.Draw(Image.new("L", (1, 1))).textbbox((0, 0), text, font=text_font)
    return bbox, bbox[2] - bbox[0], bbox[3] - bbox[1]


def draw_text(image, xy, text, text_font, fill):
    bbox, _, _ = text_size(text, text_font)
    ImageDraw.Draw(image).text((xy[0] - bbox[0], xy[1] - bbox[1]), text, font=text_font, fill=fill)


def draw_centered_text(image, y, text, text_font, fill):
    _, width, _ = text_size(text, text_font)
    draw_text(image, ((image.width - width) // 2, y), text, text_font, fill)


def fitted_font(text, max_width, preferred_size, minimum_size=6):
    for size in range(preferred_size, minimum_size - 1, -1):
        candidate = font(size)
        if text_size(text, candidate)[1] <= max_width:
            return candidate
    return font(minimum_size)


def render_track(text):
    text_font = font(28)
    bbox, width, height = text_size(text, text_font)
    image = Image.new("L", (width + 12, height + 6), 0)
    ImageDraw.Draw(image).text((6 - bbox[0], 3 - bbox[1]), text, font=text_font, fill=255)
    return image


def render_status():
    image = Image.new("L", (126, 42), 0)
    draw_text(image, (0, 0), "再生時間", fitted_font("再生時間", 126, 20), 255)
    draw_text(image, (0, 26), "PLAYBACK TIME", fitted_font("PLAYBACK TIME", 126, 11), 255)
    return image


def render_evangelion():
    image = Image.new("L", (124, 26), 0)
    text = "EVANGELION"
    text_font = fitted_font(text, 120, 19)
    bbox, width, height = text_size(text, text_font)
    mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(mask).text((-bbox[0], -bbox[1]), text, font=text_font, fill=255)
    content = mask.crop(mask.getbbox())
    image.paste(content, ((image.width - content.width) // 2,
                          (image.height - content.height) // 2))
    return image


def render_internal(bars=3):
    image = Image.new("RGBA", (90, 40), (0, 0, 0, 0))
    draw_text(image, (2, 0), "内部", fitted_font("内部", 66, 28), ORANGE)
    draw_text(image, (2, 29), "INTERNAL", fitted_font("INTERNAL", 66, 10), ORANGE)
    draw = ImageDraw.Draw(image)
    for y in (1, 14, 27)[:bars]:
        draw.polygon([(74, y), (89, y), (82, min(y + 11, 39)), (68, min(y + 11, 39))], fill=MAGENTA)
    return image


def render_system():
    image = Image.new("RGBA", (90, 26), (0, 0, 0, 0))
    first_line = "音楽再生"
    second_line = "システム"
    draw_centered_text(image, 0, first_line, fitted_font(first_line, 88, 12), ORANGE)
    draw_centered_text(image, 14, second_line, fitted_font(second_line, 88, 11), ORANGE)
    return image


def render_mode_badge(title, subtitle):
    image = Image.new("RGBA", (90, 40), (0, 0, 0, 0))
    draw_text(image, (2, 8 if not subtitle else 0), title,
              fitted_font(title, 65, 28), ORANGE)
    if subtitle:
        draw_text(image, (2, 29), subtitle, fitted_font(subtitle, 66, 10), ORANGE)
    draw = ImageDraw.Draw(image)
    for y in (1, 14, 27):
        draw.polygon([(74, y), (89, y), (82, min(y + 11, 39)),
                      (68, min(y + 11, 39))], fill=MAGENTA)
    return image


def render_mode_line(text, subtitle, width=126):
    image = Image.new("L", (width, 42), 0)
    draw_text(image, (0, 10 if not subtitle else 0), text,
              fitted_font(text, width, 20), 255)
    if subtitle:
        draw_text(image, (0, 26), subtitle, fitted_font(subtitle, width, 11), 255)
    return image


def render_instruction(text):
    image = Image.new("L", (220, 35), 0)
    draw_centered_text(image, 0, text, fitted_font(text, 216, 18), 255)
    return image


def build_assets():
    return [
        Asset("eva_text_status", render_status(), "A8"),
        Asset("eva_text_evangelion", render_evangelion(), "A8"),
        Asset("eva_text_internal_jp", render_internal(), "ARGB8888"),
        Asset("eva_text_internal_battery_0", render_internal(0), "ARGB8888"),
        Asset("eva_text_internal_battery_1", render_internal(1), "ARGB8888"),
        Asset("eva_text_internal_battery_2", render_internal(2), "ARGB8888"),
        Asset("eva_text_system_jp", render_system(), "ARGB8888"),
        Asset("eva_text_wifi_badge", render_mode_badge("WiFi", ""), "ARGB8888"),
        Asset("eva_text_volume_badge", render_mode_badge("音量", "VOLUME"), "ARGB8888"),
        Asset("eva_text_wifi_status", render_mode_line("音楽管理", ""), "A8"),
        Asset("eva_text_volume_status", render_mode_line("音量調節", "OUTPUT LEVEL"), "A8"),
        Asset("eva_text_volume_instruction", render_instruction("上鍵增大  下鍵減小"), "A8"),
        Asset("eva_text_track_0", render_track("残酷な天使のテーゼ"), "A8"),
        Asset("eva_text_track_1", render_track("One Last Kiss"), "A8"),
        Asset("eva_text_track_2", render_track("Beautiful World"), "A8"),
    ]


def asset_bytes(asset):
    if asset.color_format == "A8":
        return list(asset.image.convert("L").getdata()), asset.image.width
    data = []
    for r, g, b, a in asset.image.convert("RGBA").getdata():
        data.extend((b, g, r, a))
    return data, asset.image.width * 4


def build_source(assets):
    lines = ['#include "eva_text_assets.h"', ""]
    for asset in assets:
        data, stride = asset_bytes(asset)
        data_name = f"{asset.name}_data"
        lines.append(f"static const uint8_t {data_name}[] = {{")
        for i in range(0, len(data), 16):
            lines.append("    " + ", ".join(f"0x{value:02X}" for value in data[i:i + 16]) + ",")
        lines.extend([
            "};",
            "",
            f"const lv_image_dsc_t {asset.name} = {{",
            "    .header.magic = LV_IMAGE_HEADER_MAGIC,",
            f"    .header.cf = LV_COLOR_FORMAT_{asset.color_format},",
            "    .header.flags = 0,",
            f"    .header.w = {asset.image.width},",
            f"    .header.h = {asset.image.height},",
            f"    .header.stride = {stride},",
            f"    .data_size = {len(data)},",
            f"    .data = {data_name},",
            "};",
            "",
        ])
    return "\n".join(lines)


def build_header(assets):
    lines = ["#pragma once", "", '#include "lvgl.h"', ""]
    lines.extend(f"extern const lv_image_dsc_t {asset.name};" for asset in assets)
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", default="main/eva_text_assets.h")
    parser.add_argument("--source", default="main/eva_text_assets.c")
    args = parser.parse_args()

    assets = build_assets()
    Path(args.header).write_text(build_header(assets), encoding="utf-8")
    Path(args.source).write_text(build_source(assets), encoding="utf-8")
    for asset in assets:
        print(f"{asset.name}: {asset.image.width}x{asset.image.height} {asset.color_format}")


if __name__ == "__main__":
    main()
