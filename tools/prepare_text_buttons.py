import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_PATH = r"C:\Windows\Fonts\msgothic.ttc"

def render_text(text, font_size):
    font = ImageFont.truetype(FONT_PATH, font_size, index=0)

    probe = Image.new("L", (1, 1), 0)
    draw = ImageDraw.Draw(probe)
    bbox = draw.textbbox((0, 0), text, font=font)
    pad = 1
    width = bbox[2] - bbox[0] + pad * 2
    height = bbox[3] - bbox[1] + pad * 2

    mask = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(mask)
    draw.text((pad - bbox[0], pad - bbox[1]), text, font=font, fill=255)

    return mask


def alpha_bytes(img):
    return list(img.convert("L").getdata())


def build_source(labels):
    lines = ['#include "eva_text_buttons.h"', ""]
    descriptors = []
    for name, text, size in labels:
        img = render_text(text, size)
        data = alpha_bytes(img)
        data_name = f"{name}_data"
        lines.append(f"static const uint8_t {data_name}[] = {{")
        for i in range(0, len(data), 16):
            chunk = ", ".join(f"0x{byte:02X}" for byte in data[i:i + 16])
            lines.append(f"    {chunk},")
        lines.extend([
            "};",
            "",
            f"const lv_image_dsc_t {name} = {{",
            "    .header.magic = LV_IMAGE_HEADER_MAGIC,",
            "    .header.cf = LV_COLOR_FORMAT_A8,",
            "    .header.flags = 0,",
            f"    .header.w = {img.width},",
            f"    .header.h = {img.height},",
            f"    .header.stride = {img.width},",
            f"    .data_size = {len(data)},",
            f"    .data = {data_name},",
            "};",
            "",
        ])
        descriptors.append((name, img.width, img.height))
    return "\n".join(lines), descriptors


def build_header(descriptors):
    lines = [
        "#pragma once",
        "",
        '#include "lvgl.h"',
        "",
    ]
    for name, _, _ in descriptors:
        lines.append(f"extern const lv_image_dsc_t {name};")
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--font-size", type=int, default=18)
    parser.add_argument("--header", default="main/eva_text_buttons.h")
    parser.add_argument("--source", default="main/eva_text_buttons.c")
    args = parser.parse_args()

    labels = [
        ("eva_text_btn_prev", "PREV", args.font_size),
        ("eva_text_btn_play", "PLAY", args.font_size),
        ("eva_text_btn_next", "NEXT", args.font_size),
        ("eva_text_btn_pause", "PAUSE", args.font_size),
    ]

    source, descriptors = build_source(labels)
    header = build_header(descriptors)

    Path(args.header).write_text(header, encoding="utf-8")
    Path(args.source).write_text(source, encoding="utf-8")
    for name, w, h in descriptors:
        print(f"{name}: {w}x{h}")


if __name__ == "__main__":
    main()
