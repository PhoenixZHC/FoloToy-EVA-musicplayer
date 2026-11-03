"""运行生产播放循环；仅替换 HTTP、解码器、时钟和 I2S 设备边界。"""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[1]


def test_network_jitter_does_not_block_buffered_audio(tmp_path):
    compiler = shutil.which('cc')
    if compiler:
        cc = [compiler]
    else:
        dev = Path.home() / '.local/share/eva-dev-tools/usr'
        if not (dev / 'bin/tcc').is_file():
            pytest.skip('host C compiler required')
        cc = [str(dev / 'bin/tcc'), '-B' + str(dev / 'lib/x86_64-linux-gnu/tcc'),
              '-I' + str(dev / 'include'), '-I' + str(dev / 'include/x86_64-linux-gnu'),
              '-L' + str(dev / 'lib/x86_64-linux-gnu')]
    source = (ROOT / 'main/eva_radio_player.c').read_text(encoding='utf-8')
    start = source.index('typedef struct {\n    int64_t started_us;')
    end = source.index('} stream_diagnostics_t;', start) + len('} stream_diagnostics_t;')
    diagnostics = source[start:end]
    start = source.index('static esp_err_t play_stream(')
    end = source.index('\nstatic void radio_task(', start)
    (tmp_path / 'radio_player_under_test.inc').write_text(diagnostics + '\n' + source[start:end])
    binary = tmp_path / 'radio_player'
    subprocess.run(cc + ['-I' + str(tmp_path), '-I' + str(ROOT / 'main'),
                        str(ROOT / 'tests/test_eva_radio_player.c'),
                        str(ROOT / 'main/eva_radio_stream.c'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
