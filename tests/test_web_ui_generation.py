"""覆盖旧网页生成物遗漏配网入口的真实故障。"""
import gzip
from pathlib import Path
import sys
import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from check_web_ui import INPUTS, source_digest, validate


def fixture_header(root, page, digest):
    data = gzip.compress(page.encode())
    (root / 'main/web_ui.h').write_text(
        f'#define WEB_UI_SOURCE_SHA256 "{digest}"\n'
        f'const unsigned char WEB_UI_HTML[] = {{{",".join(map(str, data))}}};\n'
        f'const unsigned int WEB_UI_HTML_LEN = {len(data)};\n')


def test_stale_or_missing_network_page_is_rejected(tmp_path):
    (tmp_path / 'main').mkdir()
    for name in INPUTS:
        (tmp_path / name).write_text('source')
    page = '<h2>网络电台配网</h2><iframe src="/radio/setup"></iframe>'
    fixture_header(tmp_path, page, source_digest(tmp_path))
    assert validate(tmp_path) == page
    (tmp_path / INPUTS[0]).write_text('updated')
    with pytest.raises(ValueError, match='过期'):
        validate(tmp_path)
    fixture_header(tmp_path, '<h2>音乐管理</h2>', source_digest(tmp_path))
    with pytest.raises(ValueError, match='配网入口'):
        validate(tmp_path)


def test_actual_embedded_page_contains_setup():
    if not (ROOT / 'main/web_ui.h').exists():
        pytest.skip('requires locally generated web assets')
    assert '/radio/setup' in validate(ROOT)
