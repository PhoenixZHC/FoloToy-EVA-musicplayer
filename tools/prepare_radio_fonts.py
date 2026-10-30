"""生成电台/设置的最小中文子集；缺字或工具缺失时明确失败。"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]

def required_characters():
    chars = set()
    for name in ('eva_player.c', 'eva_radio_view.c', 'eva_radio_catalog.c'):
        source = (ROOT / 'main' / name).read_text(encoding='utf-8')
        for literal in re.findall('"([^"]*)"', source):
            chars.update(c for c in literal if ord(c) > 126)
    return ''.join(sorted(chars))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font', default=os.environ.get('EVA_CJK_FONT_PATH'))
    parser.add_argument('--font-index', type=int, default=0)
    args = parser.parse_args()
    if not args.font:
        parser.error('需要 --font 或 EVA_CJK_FONT_PATH，建议使用 Noto Sans SC 字体')
    chars = required_characters()
    font = TTFont(args.font, fontNumber=args.font_index)
    missing = [c for c in chars + ''.join(map(chr, range(32, 127))) if ord(c) not in font.getBestCmap()]
    if missing:
        parser.error('中文字库缺字：' + ''.join(missing))
    npx = shutil.which('npx.cmd' if os.name == 'nt' else 'npx')
    if not npx:
        parser.error('未找到 npx；请先安装 Node.js 并将其加入 PATH')
    # 字体集先提取指定字体，转换器无需猜测 TTC 的地区字形。
    local = ROOT / 'assets/fonts'
    local.mkdir(parents=True, exist_ok=True)
    prepared = local / 'radio-source.otf'
    font.save(prepared)
    for size in (16, 20):
        name = f'eva_font_radio_{size}'
        subprocess.run([npx, '--yes', '--package', 'lv_font_conv@1.5.3', 'lv_font_conv',
            '--font', str(prepared), '--range', '0x20-0x7e', '--symbols', chars,
            '--size', str(size), '--bpp', '2', '--no-compress', '--format', 'lvgl',
            '--lv-include', 'lvgl.h', '--lv-font-name', name,
            '-o', str(ROOT / 'main' / (name + '.c'))], check=True)
    print(f'中文子集生成成功：{len(chars)} 个非 ASCII 字符，16/20 px，2bpp')

if __name__ == '__main__':
    main()
