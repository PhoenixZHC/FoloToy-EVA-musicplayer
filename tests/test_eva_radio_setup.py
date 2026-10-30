"""Exercise actual setup handlers with host-only Wi-Fi/HTTP stubs."""
import os
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[1]

def test_scan_and_credentials_handlers(tmp_path):
    idf = Path(os.environ.get('IDF_PATH', Path.home()/'.espressif/esp-idf-v5.5.4'))
    cjson = idf/'components/json/cJSON'
    if not (cjson/'cJSON.c').is_file():
        pytest.skip('ESP-IDF cJSON sources required')
    compiler = shutil.which('cc')
    if compiler:
        cc = [compiler]
    else:
        dev = Path.home()/'.local/share/eva-dev-tools/usr'
        if not (dev/'bin/tcc').is_file():
            pytest.skip('host C compiler required')
        cc = [str(dev/'bin/tcc'), '-B'+str(dev/'lib/x86_64-linux-gnu/tcc'),
              '-I'+str(dev/'include'), '-I'+str(dev/'include/x86_64-linux-gnu'),
              '-L'+str(dev/'lib/x86_64-linux-gnu')]
    for name in ['esp_err','esp_http_server','esp_wifi','esp_system','esp_log']:
        (tmp_path/(name+'.h')).write_text('#include "'+str(ROOT/'tests/radio_setup_stubs.h')+'"\n')
    binary=tmp_path/'radio_setup'
    subprocess.run(cc+['-I'+str(tmp_path),'-I'+str(cjson),'-I'+str(ROOT/'main'),
        str(ROOT/'tests/test_eva_radio_setup.c'),str(ROOT/'main/eva_radio_stream.c'),
        str(cjson/'cJSON.c'),'-lm','-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
