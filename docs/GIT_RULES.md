# Git Rules

本项目适合公开源代码，但不要把本地素材、构建产物、完整固件和协作临时记录一起提交。

## 应该提交

- `README.md`、`README.zh_CN.md`
- `ARCHITECTURE.md`
- `CONTRIBUTING.md`
- `NOTICE.md`
- `docs/*.md`
- `components/bsp/**`
- 播放器源码、`audio_catalog.*`、`fam1_format.*`、`audio_adpcm.js`、`web_ui.html`、`web_style.css`、`eva_font_matisse.h`；生成的字形与网页头文件除外
- `main/ui_pixel*.c`、`main/ui_pixel*.h`
- `main/main.c`
- `tools/*.py`、`tools/*.ps1`
- `tests/**`
- `partitions.csv`
- `sdkconfig.defaults`
- `CMakeLists.txt` 和各级 `CMakeLists.txt`
- `assets/audio/README.md`
- `assets/promo/` 中有权发布的图片

## 不要提交

| 类型 | 例子 | 原因 |
| --- | --- | --- |
| 原始音乐 | `*.mp3`, `*.wav`, `*.flac`, `*.m4a` | 可能有版权，公开仓库不能直接分发 |
| 生成音频 | `assets/audio/*.adpcm` | 是从本地音乐生成的，仍可能包含版权内容 |
| 字体及生成字形 | 自备 OTF/TTF、`main/eva_text_assets.c/.h`、`main/eva_text_buttons.c/.h`、`main/eva_font_matisse_14.c`、`main/eva_font_matisse_20.c`、`main/web_ui.h` | 均含本地字体生成的数据；用户明确要求不上传字体 |
| 完整固件 | `release/*.bin`, `*.bin` | bin 里会内嵌音乐和 Logo |
| ESP-IDF 构建目录 | `build/`, `managed_components/` | 可重新生成，体积大 |
| 本机配置 | `sdkconfig`, `sdkconfig.old`, `.venv/`, `.vscode/`, `.idea/` | 只适合本机，不适合别人复用 |
| 依赖锁和缓存 | `dependencies.lock`, `__pycache__/`, `.pytest_cache/` | 可重新生成 |
| 本地协作记录 | `CONTEXT.md`, `docs/superpowers/` | 是本机开发过程记录，不是开源用户文档 |
| 测试可执行文件 | `build_test_*`, `*.exe`, `*.o`, `*.obj` | 编译输出，不是源码 |

## 提交前检查

先看有哪些文件会被提交：

```powershell
git status --short
```

再看 Git 实际跟踪了哪些文件：

```powershell
git ls-files
```

如果输出里出现下面这些内容，先不要提交：

```text
*.mp3
*.wav
*.flac
*.m4a
assets/audio/*.adpcm
FOT-MatissePro-EB.otf
main/eva_text_assets.*
main/eva_text_buttons.*
main/eva_font_matisse_14.c
main/eva_font_matisse_20.c
main/web_ui.h
release/*.bin
CONTEXT.md
docs/superpowers/
build/
managed_components/
```

## 如果文件已经被 Git 跟踪

`.gitignore` 只能阻止新文件被加入 Git，不能自动移除已经被跟踪的文件。

如果不该上传的文件已经出现在 `git ls-files` 中，应先从 Git 索引里移除，但保留本地文件：

```powershell
git rm --cached path\to\file
```

多个文件批量处理前要逐项确认，避免误移除需要公开的源码。

## 图片资源规则

独立于字体的图片资源可以提交，例如：

```text
main/eva_logo_assets.c
main/eva_logo_assets.h
assets/promo/*.png
```

字体生成的文字图片和网页 WOFF 即使以 C 数组保存，也属于字体衍生数据，不提交。克隆仓库后使用自备字体运行 `tools/prepare_local_font.ps1` 生成；详见 `docs/ASSET_PREPARATION.md`。

新增 `.png`、`.jpg`、`.jpeg`、`.webp`、`.bmp`、`.svg` 时也要检查素材来源；音乐、生成音频、字体及其生成字形、完整固件镜像不提交。
