"""检查嵌入网页与源码一致，阻止旧资源进入固件。"""
import argparse
import gzip
import hashlib
from pathlib import Path
import re

INPUTS = ('main/web_ui.html', 'main/web_style.css', 'main/audio_adpcm.js',
          'main/eva_logo_assets.c')


def source_digest(root):
    digest = hashlib.sha256()
    for name in INPUTS:
        digest.update(name.encode() + b'\0')
        digest.update((root / name).read_bytes())
        digest.update(b'\0')
    return digest.hexdigest()


def validate(root):
    header = (root / 'main/web_ui.h').read_text(encoding='utf-8')
    expected = source_digest(root)
    if f'#define WEB_UI_SOURCE_SHA256 "{expected}"' not in header:
        raise ValueError('网页资源已过期，请使用本地字体重新运行 tools/gen_web_ui.py')
    match = re.search(r'WEB_UI_HTML\[\]\s*=\s*\{([^}]*)\}', header)
    if not match:
        raise ValueError('网页资源缺少 WEB_UI_HTML')
    packed = bytes(int(value) for value in match.group(1).split(','))
    length = re.search(r'WEB_UI_HTML_LEN\s*=\s*(\d+)', header)
    if not length or int(length.group(1)) != len(packed):
        raise ValueError('网页资源长度不一致')
    page = gzip.decompress(packed).decode('utf-8')
    if '/radio/setup' not in page or '网络电台配网' not in page:
        raise ValueError('嵌入网页缺少网络电台配网入口')
    return page


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        validate(args.root)
    except (OSError, ValueError, EOFError) as error:
        parser.exit(1, f'{error}\n')
    print('嵌入网页源码校验通过（含配网入口）')
