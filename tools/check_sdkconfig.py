"""核对有效 SDK 配置是否符合仓库默认值，阻止旧配置进入固件。"""
import argparse
from pathlib import Path
import re


def read_config(path):
    values = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.startswith('CONFIG_') and '=' in line:
            key, value = line.split('=', 1)
            values[key] = value
        else:
            match = re.fullmatch(r'# (CONFIG_\w+) is not set', line)
            if match:
                values[match.group(1)] = 'n'
    return values


def differences(defaults, actual):
    expected, current = read_config(defaults), read_config(actual)
    return [(key, value, current.get(key, '<missing>'))
            for key, value in expected.items() if current.get(key) != value]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--defaults', type=Path, required=True)
    parser.add_argument('--config', type=Path, required=True)
    args = parser.parse_args()
    try:
        drift = differences(args.defaults, args.config)
    except OSError as error:
        parser.exit(1, f'{error}\n')
    if drift:
        detail = '\n'.join(f'{key}: expected {expected}, actual {actual}'
                           for key, expected, actual in drift)
        parser.exit(1, 'SDK 配置与仓库不一致，请用 tools/idf_linux.py 在独立目录重新生成：\n' + detail + '\n')
    print('SDK 配置与仓库 sdkconfig.defaults 一致')
