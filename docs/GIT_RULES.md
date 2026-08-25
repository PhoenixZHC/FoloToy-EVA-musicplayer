# Git Rules

本项目适合公开源代码，但不要把本地素材、构建产物、完整固件和协作临时记录一起提交。

## 应该提交

- `README.md`、`README.zh_CN.md`
- `ARCHITECTURE.md`
- `CONTRIBUTING.md`
- `NOTICE.md`
- `docs/*.md`
- `components/bsp/**`
- `main/eva_*.c`、`main/eva_*.h`，包括生成后的图片/Logo 资源源码
- `main/ui_pixel*.c`、`main/ui_pixel*.h`
- `main/main.c`
- `tools/*.py`、`tools/*.ps1`
- `tests/**`
- `partitions.csv`
- `sdkconfig.defaults`
- `CMakeLists.txt` 和各级 `CMakeLists.txt`
- `assets/audio/README.md`

## 不要提交

| 类型 | 例子 | 原因 |
| --- | --- | --- |
| 原始音乐 | `*.mp3`, `*.wav`, `*.flac`, `*.m4a` | 可能有版权，公开仓库不能直接分发 |
| 生成音频 | `assets/audio/*.adpcm` | 是从本地音乐生成的，仍可能包含版权内容 |
| 完整固件 | `release/*.bin`, `*.bin` | bin 里会内嵌音乐和 Logo |
| ESP-IDF 构建目录 | `build/`, `managed_components/` | 可重新生成，体积大 |
| 本机配置 | `sdkconfig`, `sdkconfig.old`, `.vscode/`, `.idea/` | 只适合本机，不适合别人复用 |
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

本项目的图片类资源需要上传，包括：

```text
main/eva_logo_assets.c
main/eva_logo_assets.h
main/eva_text_assets.c
main/eva_text_assets.h
main/eva_text_buttons.c
main/eva_text_buttons.h
```

原因是设备运行时需要这些生成后的 UI 图片数据。没有这些文件，公开仓库里的播放器界面不完整。

如果以后增加 `.png`、`.jpg`、`.jpeg`、`.webp`、`.bmp`、`.svg` 等图片源文件，默认也可以提交；只有音乐、生成音频和完整固件镜像默认不提交。
