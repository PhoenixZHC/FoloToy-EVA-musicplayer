"""Package an 8 MiB factory image with deletable songs in the writable FAT partition.

Run with the ESP-IDF Python environment after idf.py build. Does not flash hardware.
"""

import argparse
import hashlib
import json
import os
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH_SIZE = 8 * 1024 * 1024


def inspect_song(data):
    if not 40 <= len(data) <= 2560 * 1024:
        raise ValueError('Invalid FAM1 size')
    magic, version, header, rate, channels, codec, block, title_len, samples, duration, size, crc = (
        struct.unpack_from('<4sHHHBBHHIIII', data))
    if (magic != b'FAM1' or version != 1 or header != 32 + title_len or
            title_len > 64 or rate not in (8000, 12000) or channels != 1 or
            codec != 1 or block != 512 or not samples or duration > 360000 or
            duration != (samples * 1000 + rate // 2) // rate or size != len(data) - header):
        raise ValueError('Invalid FAM1 header')
    title = data[32:header].decode('utf-8')
    if not title or any(ord(c) < 32 or ord(c) == 127 for c in title):
        raise ValueError('Invalid FAM1 title')
    if zlib.crc32(data[header:]) != crc:
        raise ValueError('FAM1 CRC mismatch')
    offset, count = header, 0
    while offset < len(data):
        if len(data) - offset < 8:
            raise ValueError('Truncated FAM1 block')
        _, step, flags, n, encoded = struct.unpack_from('<hBBHH', data, offset)
        if (step > 88 or flags or not 1 <= n <= 512 or encoded != n // 2 or
                offset + 8 + encoded > len(data) or count + n > samples or
                (count + n < samples and n != 512)):
            raise ValueError('Invalid FAM1 block')
        count += n
        offset += 8 + encoded
    if count != samples:
        raise ValueError('FAM1 sample count mismatch')
    return {'title': title, 'sample_rate': rate, 'duration_ms': duration, 'bytes': len(data)}


def inspect_title(data):
    if len(data) < 8:
        raise ValueError('Truncated title mask')
    magic, width, height = struct.unpack_from('<4sHH', data)
    if magic != b'ETT1' or not 1 <= width <= 768 or height != 34 or len(data) != 8 + width * height:
        raise ValueError('Invalid title mask')


def load_music(directory):
    count = len(list(directory.glob('a[0-9][0-9][0-9].fam')))
    if not 1 <= count <= 32:
        raise ValueError('Expected 1 to 32 songs')
    expected = {f'{kind}{i:03}.{ext}' for i in range(count)
                for kind, ext in [('a', 'fam'), ('t', 'bin')]}
    if {p.name for p in directory.iterdir()} != expected:
        raise ValueError('Music directory must contain consecutive aNNN.fam/tNNN.bin pairs only')
    files = {name: (directory / name).read_bytes() for name in sorted(expected)}
    tracks = []
    for i in range(count):
        tracks.append(inspect_song(files[f'a{i:03}.fam']))
        inspect_title(files[f't{i:03}.bin'])
    return files, tracks


def merge_segments(segments, size=FLASH_SIZE):
    image = bytearray(b'\xff' * size)
    end = 0
    for offset, data in sorted(segments):
        if offset < end or offset < 0 or offset + len(data) > size:
            raise ValueError('Flash segments overlap or exceed flash capacity')
        image[offset:offset + len(data)] = data
        end = offset + len(data)
    return image


def verify_fat(image, files):
    from wl_fatfsgen import remove_wl
    from fatfs_utils.boot_sector import BootSector
    from fatfs_utils.entry import Entry
    from fatfs_utils.fat import FAT
    boot = BootSector()
    plain = remove_wl(image)
    boot.parse_boot_sector(plain)
    state = boot.boot_sector_state
    fat = FAT(state, init_=False)
    found = {}
    start = state.root_directory_start
    for offset in range(start, start + state.root_dir_sectors_cnt * state.sector_size, 32):
        if plain[offset] == 0:
            break
        entry = Entry.ENTRY_FORMAT_SHORT_NAME.parse(plain[offset:offset + 32])
        if entry['DIR_Attr'] != Entry.ATTR_ARCHIVE:
            raise ValueError('Unexpected FAT directory entry')
        name = (entry['DIR_Name'].rstrip() + '.' + entry['DIR_Name_ext'].rstrip()).lower()
        found[name] = fat.get_chained_content(cluster_id_=Entry.get_cluster_id(entry),
                                              size=entry['DIR_FileSize'])
    if found != files:
        raise ValueError('FAT round-trip did not preserve all song and title bytes')
    free_clusters = sum(fat.get_cluster_value(i) == 0 for i in range(2, state.clusters))
    return free_clusters * state.sector_size * state.sectors_per_cluster


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--music', type=Path, default=ROOT / 'assets/preset_music')
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--idf-path', type=Path, default=os.environ.get('IDF_PATH'))
    args = parser.parse_args()
    if not args.idf_path:
        parser.error('Activate ESP-IDF or provide --idf-path')
    for path in [args.output, args.output.with_suffix('.music.bin'),
                 args.output.with_suffix('.sha256'), args.output.with_suffix('.json')]:
        if path.exists():
            parser.error(f'Refusing to overwrite existing package: {path}')
    sys.path.insert(0, str(args.idf_path / 'components/fatfs'))
    sys.path.insert(0, str(args.idf_path / 'components/partition_table'))
    from gen_esp32part import PartitionTable
    from wl_fatfsgen import WLFATFS
    config = json.loads((args.build / 'config/sdkconfig.json').read_text(encoding='utf-8'))
    if config['WL_SECTOR_SIZE'] != 4096 or config['ESPTOOLPY_FLASHSIZE'] != '8MB':
        raise ValueError('This factory packager requires 4096-byte WL sectors and 8MB flash')
    flash = json.loads((args.build / 'flasher_args.json').read_text(encoding='utf-8'))
    if flash['extra_esptool_args']['chip'] != 'esp32c3':
        raise ValueError('Expected ESP32-C3 build')
    partitions = PartitionTable.from_binary((args.build / flash['partition-table']['file']).read_bytes())
    music = next(p for p in partitions if p.name == 'music')
    app = next(p for p in partitions if p.name == 'factory')
    if music.offset != 0x210000 or music.size != 0x5f0000 or music.type != 1 or music.subtype != 0x81:
        raise ValueError('Unexpected music partition layout')
    files, tracks = load_music(args.music)
    filesystem = WLFATFS(size=music.size, sector_size=4096, fat_tables_cnt=2,
                        sectors_per_cluster=1)
    filesystem.plain_fatfs.generate(str(args.music.resolve()))
    filesystem.init_wl()
    music_image = bytes(filesystem.fatfs_binary_image)
    if len(music_image) != music.size:
        raise ValueError('Music image size does not match partition')
    free_bytes = verify_fat(music_image, files)
    segments = [(int(offset, 0), (args.build / filename).read_bytes())
                for offset, filename in flash['flash_files'].items()]
    app_bytes = (args.build / flash['app']['file']).read_bytes()
    if int(flash['app']['offset'], 0) != app.offset or len(app_bytes) > app.size:
        raise ValueError('Application does not fit factory partition')
    segments.append((music.offset, music_image))
    image = merge_segments(segments)
    # No NVS seed or first-boot restoration: a valid FAT volume mounts as-is.
    # Deleting a preset therefore behaves exactly like deleting an upload.
    digest = hashlib.sha256(image).hexdigest()
    report = {'image': args.output.name, 'bytes': len(image), 'sha256': digest,
              'music_offset': music.offset, 'music_partition_bytes': music.size,
              'music_free_bytes': free_bytes,
              'song_and_title_bytes': sum(map(len, files.values())),
              'app_bytes': len(app_bytes), 'app_free_bytes': app.size - len(app_bytes),
              'tracks': tracks,
              'files_sha256': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
              'fat_round_trip_verified': True, 'hardware_verified': False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.with_suffix('.music.bin').write_bytes(music_image)
    args.output.write_bytes(image)
    if args.output.read_bytes() != image:
        raise IOError('Written factory image differs from verified image')
    args.output.with_suffix('.sha256').write_text(f'{digest}  {args.output.name}\n', encoding='utf-8')
    args.output.with_suffix('.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
