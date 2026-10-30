"""Run settings model and actual NVS adapter on the host."""
from pathlib import Path
import shutil
import subprocess
import pytest
ROOT=Path(__file__).resolve().parents[1]
@pytest.mark.parametrize('storage',[False,True])
def test_settings_behaviour(tmp_path,storage):
    compiler=shutil.which('cc')
    if compiler:
        cc=[compiler]
    else:
        dev=Path.home()/'.local/share/eva-dev-tools/usr'
        if not (dev/'bin/tcc').is_file(): pytest.skip('host compiler required')
        cc=[str(dev/'bin/tcc'),'-B'+str(dev/'lib/x86_64-linux-gnu/tcc'),
            '-I'+str(dev/'include'),'-I'+str(dev/'include/x86_64-linux-gnu'),'-L'+str(dev/'lib/x86_64-linux-gnu')]
    sources=[ROOT/'main/eva_settings.c']
    if storage:
        (tmp_path/'esp_err.h').write_text('#pragma once\n#include <stddef.h>\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_INVALID_ARG 0x102\n#define ESP_ERR_NVS_NOT_FOUND 0x1102\n')
        (tmp_path/'nvs.h').write_text('#pragma once\n#include "esp_err.h"\ntypedef int nvs_handle_t;\n#define NVS_READONLY 0\n#define NVS_READWRITE 1\nint nvs_open(const char*,int,nvs_handle_t*);\nint nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);\nint nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);\nint nvs_commit(nvs_handle_t);\nvoid nvs_close(nvs_handle_t);\n')
        sources += [ROOT/'main/eva_settings_store.c',ROOT/'tests/test_eva_settings_store.c']
    else: sources += [ROOT/'tests/test_eva_settings.c']
    binary=tmp_path/'settings'
    subprocess.run(cc+['-I'+str(tmp_path),'-I'+str(ROOT/'main')]+[str(p) for p in sources]+['-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
