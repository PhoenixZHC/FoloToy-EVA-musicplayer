import importlib.util
import struct
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('package_factory', ROOT / 'tools/package_factory.py')
package = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(package)


class FactoryPackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.song = subprocess.run([
            'node', '-e',
            "const c=require(process.argv[1]); process.stdout.write(c.encodePcm16ToFam1(new Int16Array(513),12000,'Test'));",
            str(ROOT / 'main/audio_adpcm.js')], check=True, stdout=subprocess.PIPE).stdout

    def test_browser_encoder_compatible(self):
        info = package.inspect_song(self.song)
        self.assertEqual(info['title'], 'Test')
        self.assertEqual(info['sample_rate'], 12000)

    def test_reject_corrupt_audio(self):
        data = bytearray(self.song)
        data[-1] ^= 1
        with self.assertRaisesRegex(ValueError, 'CRC'):
            package.inspect_song(data)

    def test_reject_truncated_audio(self):
        with self.assertRaises(ValueError):
            package.inspect_song(self.song[:-1])

    def test_reject_overlong_song(self):
        data = bytearray(self.song)
        struct.pack_into('<I', data, 20, 360001)
        with self.assertRaises(ValueError):
            package.inspect_song(data)

    def test_title_dimensions(self):
        package.inspect_title(b'ETT1' + struct.pack('<HH', 16, 34) + bytes(16 * 34))
        with self.assertRaises(ValueError):
            package.inspect_title(b'ETT1' + struct.pack('<HH', 769, 34) + bytes(769 * 34))

    def test_reject_truncated_title(self):
        with self.assertRaises(ValueError):
            package.inspect_title(b'ETT1' + struct.pack('<HH', 16, 34))

    def test_merge_preserves_segments_and_blank_regions(self):
        self.assertEqual(package.merge_segments([(4, b'cd'), (0, b'ab')], 8),
                         b'ab\xff\xffcd\xff\xff')

    def test_reject_overlapping_segments(self):
        with self.assertRaises(ValueError):
            package.merge_segments([(0, b'abc'), (2, b'd')], 8)

    def test_reject_flash_overflow(self):
        with self.assertRaises(ValueError):
            package.merge_segments([(7, b'ab')], 8)


if __name__ == '__main__':
    unittest.main()
