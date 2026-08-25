import re
import sys
from pathlib import Path

# 解析 .c 文件里的 RGB565 数据数组和描述符,渲染成 ASCII 便于人工核对字形。
def parse_asset(header_path, source_path, name):
    src = Path(source_path).read_text(encoding="utf-8")
    # 描述符:名为 name 的 const lv_image_dsc_t,取 w/h
    m = re.search(
        r"const lv_image_dsc_t\s+" + re.escape(name) + r"\s*=\s*\{(.*?)\};",
        src, re.S)
    if not m:
        return None
    body = m.group(1)
    w = int(re.search(r"\.header\.w\s*=\s*(\d+)", body).group(1))
    h = int(re.search(r"\.header\.h\s*=\s*(\d+)", body).group(1))
    data_name = re.search(r"\.data\s*=\s*(\w+)", body).group(1)
    # 数据数组
    dm = re.search(
        r"const uint8_t\s+" + re.escape(data_name) + r"\[\]\s*=\s*\{(.*?)\};",
        src, re.S)
    if not dm:
        dm = re.search(
            r"static const uint8_t\s+" + re.escape(data_name) + r"\[\]\s*=\s*\{(.*?)\};",
            src, re.S)
    if not dm:
        return None
    hexes = re.findall(r"0x([0-9A-Fa-f]{2})", dm.group(1))
    data = bytes(int(x, 16) for x in hexes)
    return w, h, data


def render_ascii(w, h, data):
    # RGB565 -> 亮度,非零即亮色(文字),描点。
    lines = []
    for y in range(h):
        row = []
        for x in range(w):
            lo = data[y * w * 2 + x * 2]
            hi = data[y * w * 2 + x * 2 + 1]
            val = lo | (hi << 8)
            if val == 0:
                row.append('.')
            elif val == 0xFFFF:
                row.append('W')
            else:
                row.append('#')
        lines.append(''.join(row))
    return lines


if __name__ == "__main__":
    src = Path("main/eva_text_assets.c")
    btn = Path("main/eva_text_buttons.c")
    names = []
    # 手动列出所有资产名
    names.append(("main/eva_text_assets.c", "eva_text_internal_jp"))
    names.append(("main/eva_text_assets.c", "eva_text_system_jp"))
    names.append(("main/eva_text_assets.c", "eva_text_track_0"))
    names.append(("main/eva_text_assets.c", "eva_text_track_1"))
    names.append(("main/eva_text_assets.c", "eva_text_track_2"))
    names.append(("main/eva_text_buttons.c", "eva_text_btn_prev"))
    names.append(("main/eva_text_buttons.c", "eva_text_btn_play"))
    names.append(("main/eva_text_buttons.c", "eva_text_btn_next"))
    names.append(("main/eva_text_buttons.c", "eva_text_btn_pause"))

    for fn, name in names:
        w, h, data = parse_asset(None, fn, name)
        print(f"=== {name}  {w}x{h} ===")
        for line in render_ascii(w, h, data):
            print(line)
        print()