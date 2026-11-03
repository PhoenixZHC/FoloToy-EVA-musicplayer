"""锁定构建配置来源，防止旧根目录 sdkconfig 覆盖仓库配置。"""
from pathlib import Path
import sys
import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from check_sdkconfig import differences
from idf_linux import repository_build_args


@pytest.mark.parametrize('args, directory', [
    (['build'], 'build_linux_repo'),
    (['-B', 'build_test', 'build'], 'build_test'),
    (['--build-dir=build_other', 'size'], 'build_other'),
    (['--build-dir', 'build_separate', 'build'], 'build_separate'),
    (['-Bbuild_combined', 'build'], 'build_combined'),
])
def test_build_config_is_isolated(args, directory):
    result = repository_build_args(ROOT, args)
    assert f'SDKCONFIG={ROOT / directory / "sdkconfig"}' in result
    assert f'SDKCONFIG_DEFAULTS={ROOT / "sdkconfig.defaults"}' in result
    assert 'IDF_TARGET=esp32c3' in result
    assert f'SDKCONFIG={ROOT / "sdkconfig"}' not in result


@pytest.mark.parametrize('definition', [
    'SDKCONFIG=sdkconfig',
    '-DSDKCONFIG=sdkconfig',
    'SDKCONFIG:FILEPATH=sdkconfig',
    '-DSDKCONFIG_DEFAULTS:STRING=old.defaults',
    'IDF_TARGET=esp32',
])
def test_explicit_config_override_is_rejected(definition):
    with pytest.raises(ValueError):
        repository_build_args(ROOT, ['-D', definition, 'build'])


def test_old_config_and_disabled_options_are_checked(tmp_path):
    expected = tmp_path / 'defaults'
    actual = tmp_path / 'actual'
    expected.write_text('CONFIG_WINDOW=11520\nCONFIG_SACK=y\nCONFIG_UNUSED=n\n')
    actual.write_text('CONFIG_WINDOW=2880\n# CONFIG_SACK is not set\n# CONFIG_UNUSED is not set\n')
    assert differences(expected, actual) == [
        ('CONFIG_WINDOW', '11520', '2880'), ('CONFIG_SACK', 'y', 'n')]
    actual.write_text('CONFIG_WINDOW=11520\nCONFIG_SACK=y\n# CONFIG_UNUSED is not set\n')
    assert not differences(expected, actual)
