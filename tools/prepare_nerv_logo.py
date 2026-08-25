import argparse
from pathlib import Path

from PIL import Image


def is_red(pixel):
    r, g, b = pixel[:3]
    return r > 120 and r > g * 1.8 and r > b * 1.8


def build_logo_image(source, width, height):
    rgb = source.convert("RGB")
    red_pixels = []
    for y in range(rgb.height):
        for x in range(rgb.width):
            if is_red(rgb.getpixel((x, y))):
                red_pixels.append((x, y))

    if not red_pixels:
        raise ValueError("No red logo pixels found")

    min_x = min(x for x, _ in red_pixels)
    max_x = max(x for x, _ in red_pixels)
    min_y = min(y for _, y in red_pixels)
    max_y = max(y for _, y in red_pixels)
    cropped = rgb.crop((min_x, min_y, max_x + 1, max_y + 1))
    cropped.thumbnail((width, height), Image.Resampling.LANCZOS)

    out = Image.new("RGB", (width, height), (0, 0, 0))
    x0 = (width - cropped.width) // 2
    y0 = (height - cropped.height) // 2
    out.paste(cropped, (x0, y0))

    pixels = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b = pixels[x, y]
            if is_red((r, g, b)):
                pixels[x, y] = (220, 24, 20)
            else:
                pixels[x, y] = (0, 0, 0)
    return out


def rgb565_bytes(img):
    data = []
    for r, g, b in img.convert("RGB").getdata():
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append(value & 0xFF)
        data.append((value >> 8) & 0xFF)
    return data


def write_assets(img, header_path, source_path):
    data = rgb565_bytes(img)
    header = [
        "#pragma once",
        "",
        '#include "lvgl.h"',
        "",
        "extern const lv_image_dsc_t eva_logo_nerv;",
        "",
    ]

    source = [
        '#include "eva_logo_assets.h"',
        "",
        "static const uint8_t eva_logo_nerv_data[] = {",
    ]
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{byte:02X}" for byte in data[i:i + 16])
        source.append(f"    {chunk},")
    source.extend([
        "};",
        "",
        "const lv_image_dsc_t eva_logo_nerv = {",
        "    .header.magic = LV_IMAGE_HEADER_MAGIC,",
        "    .header.cf = LV_COLOR_FORMAT_RGB565,",
        "    .header.flags = 0,",
        f"    .header.w = {img.width},",
        f"    .header.h = {img.height},",
        f"    .header.stride = {img.width * 2},",
        f"    .data_size = {len(data)},",
        "    .data = eva_logo_nerv_data,",
        "};",
        "",
    ])

    Path(header_path).write_text("\n".join(header), encoding="utf-8")
    Path(source_path).write_text("\n".join(source), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_image")
    parser.add_argument("--width", type=int, default=180)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--header", default="main/eva_logo_assets.h")
    parser.add_argument("--source", default="main/eva_logo_assets.c")
    args = parser.parse_args()

    logo = build_logo_image(Image.open(args.input_image), args.width, args.height)
    write_assets(logo, args.header, args.source)


if __name__ == "__main__":
    main()
