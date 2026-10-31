"""运行实际按键及设置函数；仅将设备、存储和 LVGL 边界替换为主机桩。"""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[1]


def test_navigation_and_settings(tmp_path):
    compiler = shutil.which("cc")
    if compiler:
        cc = [compiler]
    else:
        dev = Path.home() / ".local/share/eva-dev-tools/usr"
        if not (dev / "bin/tcc").is_file():
            pytest.skip("host C compiler required")
        cc = [str(dev / "bin/tcc"), "-B" + str(dev / "lib/x86_64-linux-gnu/tcc"),
              "-I" + str(dev / "include"), "-I" + str(dev / "include/x86_64-linux-gnu"),
              "-L" + str(dev / "lib/x86_64-linux-gnu")]
    source = (ROOT / "main/eva_player.c").read_text(encoding="utf-8")
    functions = []
    for signature in ("static void close_settings_mode(bool network)",
                      "static void settings_button(bsp_btn_t btn, bsp_btn_ev_t ev)",
                      "static void process_source_request(void)",
                      "void eva_player_handle_button(bsp_btn_t btn, bsp_btn_ev_t ev)"):
        start = source.index(signature + "\n{")
        end = source.index("\n}", start) + 2
        functions.append(source[start:end])
    (tmp_path / "eva_navigation_under_test.inc").write_text("\n".join(functions), encoding="utf-8")
    binary = tmp_path / "navigation"
    subprocess.run(cc + ["-I" + str(tmp_path), "-I" + str(ROOT / "main"),
                        str(ROOT / "tests/test_eva_navigation.c"),
                        str(ROOT / "main/eva_settings.c"),
                        str(ROOT / "main/eva_player_model.c"),
                        str(ROOT / "main/eva_radio_catalog.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
