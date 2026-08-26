# FoloToy EVA 音乐播放器

当前固件版本：`1.1.0`。

这是一个面向 FoloToy AI Passport 的 ESP-IDF 固件工程。当前应用会直接启动到一个固定曲目的离线 EVA 风格音乐播放器，适配 240 x 320 屏幕和三个实体按键。

## 功能

- 开机后先显示 3 秒黑底、居中的红色 NERV 风格启动图。
- 进入正面 EVA 风格播放器界面。
- 默认停止播放，开机后不会自动放歌。
- 从固件内嵌的压缩音频中播放固定歌单。
- `OK` 按键用于播放和暂停。
- 暂停时长按 `OK` 会进入 NERV 标志待机页；在待机页短按 `OK` 回到原来的暂停进度。
- `UP` 按下时点亮 `PREV`，松开后切到上一首。
- `DOWN` 按下时点亮 `NEXT`，松开后切到下一首。
- 一首歌播完后，在结束时间停留 2 秒，然后自动播放下一首。
- 歌名过长时在框内单向循环滚动，像广告牌一样。

当前显示曲目：

- `残酷な天使のテーゼ`
- `One Last Kiss`
- `Beautiful World`

## 项目结构

```text
components/bsp/       板级支持包：屏幕、按键、音频、电池、I2C
main/                 EVA 播放器应用、生成后的 UI 资源、播放状态模型
assets/audio/         本地生成的 ADPCM 音频，不建议公开发布
tools/                素材转换和检查脚本
tests/                可在电脑上运行的逻辑和素材测试
docs/                 硬件、架构、素材和发布文档
partitions.csv        8 MB Flash 分区表，factory 应用分区为 4 MB
sdkconfig.defaults    ESP32-C3、USB console、LVGL、Flash 和分区默认配置
```
