"""用本机已安装的独立工具运行 ESP-IDF；不修改系统或安装依赖。"""
import os
from pathlib import Path
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    home = Path.home()
    idf = Path(os.environ.get('EVA_IDF_PATH', home / '.espressif/esp-idf-v5.5.4'))
    pyenv = Path(os.environ.get('EVA_IDF_PYTHON_ENV', home / '.espressif/python_env/idf5.5_py3.12_env'))
    tools = Path(os.environ.get('EVA_DEV_TOOLS', home / '.local/share/eva-dev-tools/usr'))
    compiler = Path(os.environ.get('EVA_RISCV_BIN', home / '.espressif/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin'))
    required = [idf / 'tools/idf.py', pyenv / 'bin/python', tools / 'bin/cmake',
                tools / 'bin/ninja', compiler / 'riscv32-esp-elf-gcc']
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        print('缺少开发工具，请核对安装路径：\n' + '\n'.join(missing), file=sys.stderr)
        return 1
    env = os.environ.copy()
    env.update(IDF_PATH=str(idf), IDF_PYTHON_ENV_PATH=str(pyenv), ESP_IDF_VERSION='5.5.4')
    env['PATH'] = os.pathsep.join([str(tools / 'bin'), str(pyenv / 'bin'),
                                   str(root / '.venv/bin'), str(compiler), env.get('PATH', '')])
    env['LD_LIBRARY_PATH'] = os.pathsep.join(filter(None, [
        str(tools / 'lib/x86_64-linux-gnu'), env.get('LD_LIBRARY_PATH', '')]))
    command = [str(pyenv / 'bin/python'), str(idf / 'tools/idf.py')]
    args = sys.argv[1:] or ['build']
    if args == ['--check']:
        for check in [command + ['--version'], [str(required[-1]), '--version'],
                      [str(tools / 'bin/cmake'), '--version'],
                      [str(tools / 'bin/ninja'), '--version'],
                      [str(pyenv / 'bin/python'), '-m', 'pip', '--version']]:
            result = subprocess.run(check, cwd=root, env=env)
            if result.returncode:
                return result.returncode
        return 0
    return subprocess.run(command + args, cwd=root, env=env).returncode


if __name__ == '__main__':
    sys.exit(main())
