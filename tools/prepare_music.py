"""Convert local songs using the upload page's encoder and create FAT title masks."""

import argparse
import json
import math
import shutil
import struct
import subprocess
from pathlib import Path

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ENCODE_JS = r"""
const fs = require('fs');
const codec = require(process.argv[1]);
const raw = fs.readFileSync(0);
if (!raw.length || raw.length % 2) throw Error('Invalid PCM data');
const pcm = new Int16Array(raw.length / 2);
for (let i = 0; i < pcm.length; i++) pcm[i] = raw.readInt16LE(i * 2);
const rate = Number(process.argv[2]);
if (pcm.length > rate * 360) throw Error('Song exceeds six minutes');
process.stdout.write(codec.encodePcm16ToFam1(pcm, rate, process.argv[3]));
"""


def validate_title(title):
    if (not title.strip() or title != title.strip() or
            len(title.encode('utf-8')) > 64 or
            any(ord(c) < 32 or ord(c) == 127 for c in title)):
        raise ValueError('Title must be nonempty, trimmed, and at most 64 UTF-8 bytes')


def render_title(title, font_path):
    validate_title(title)
    with TTFont(font_path) as source:
        cmap = source.getBestCmap()
        missing = [c for c in title if ord(c) not in cmap]
        if missing:
            raise ValueError(f'Font missing title characters: {"".join(missing)}')
    font = ImageFont.truetype(str(font_path), 29)
    left, top, right, bottom = font.getbbox(title)
    width = max(16, math.ceil(max(font.getlength(title), right - left)) + 10)
    if width > 768 or bottom - top > 34:
        raise ValueError('Title mask exceeds device dimensions (768 x 34)')
    mask = Image.new('L', (width, 34))
    ImageDraw.Draw(mask).text((5 - left, (34 - (bottom - top)) // 2 - top),
                             title, font=font, fill=255)
    return b'ETT1' + struct.pack('<HH', width, 34) + mask.tobytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--track', nargs=2, action='append', required=True,
                        metavar=('AUDIO', 'TITLE'))
    parser.add_argument('--font', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'assets/preset_music')
    parser.add_argument('--sample-rate', type=int, choices=(8000, 12000), default=12000)
    parser.add_argument('--ffmpeg', help='FFmpeg executable (or PATH/imageio_ffmpeg)')
    parser.add_argument('--node', default='node')
    args = parser.parse_args()
    if not 1 <= len(args.track) <= 32:
        parser.error('Supply 1 to 32 tracks')
    if args.output.exists() and any(args.output.iterdir()):
        parser.error('Output directory must be empty; use a new directory to avoid stale songs')
    ffmpeg = args.ffmpeg or shutil.which('ffmpeg')
    if not ffmpeg:
        import imageio_ffmpeg
        ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    # Validate all paths and glyphs before creating any output.
    masks = []
    for filename, title in args.track:
        if not Path(filename).is_file():
            raise FileNotFoundError(filename)
        masks.append(render_title(title, args.font))
    args.output.mkdir(parents=True, exist_ok=True)
    for index, ((filename, title), mask) in enumerate(zip(args.track, masks)):
        pcm = subprocess.run([ffmpeg, '-v', 'error', '-nostdin', '-i', filename,
                              '-map', '0:a:0', '-ac', '1', '-ar', str(args.sample_rate),
                              '-t', '361', '-f', 's16le', 'pipe:1'],
                             check=True, stdout=subprocess.PIPE).stdout
        fam = subprocess.run([args.node, '-e', ENCODE_JS,
                              str(ROOT / 'main/audio_adpcm.js'), str(args.sample_rate), title],
                             input=pcm, check=True, stdout=subprocess.PIPE).stdout
        title_length = struct.unpack_from('<H', fam, 14)[0]
        if fam[32:32 + title_length].decode('utf-8') != title:
            raise ValueError('Encoder changed title; supply a title without a filename extension')
        (args.output / f'a{index:03}.fam').write_bytes(fam)
        (args.output / f't{index:03}.bin').write_bytes(mask)
        print(json.dumps({'index': index, 'title': title, 'bytes': len(fam),
                          'duration_ms': struct.unpack_from('<I', fam, 20)[0],
                          'sample_rate': args.sample_rate}, ensure_ascii=False))


if __name__ == '__main__':
    main()
