"""音源取样器回归，测试数据不进入生产目录。"""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_radio_sources import candidates, consecutive_frames, frame_info


def frame(header=b"\xff\xfb\x50\x00"):
    size, _, _ = frame_info(header, 0)
    return header + bytes(size - len(header))


def test_requires_eight_complete_frames_not_a_mime_type_or_sync_word():
    data = frame()
    assert consecutive_frames(b"<html>unavailable</html>") is None
    assert consecutive_frames(data * 7 + data[:-1]) is None
    assert consecutive_frames(b"ID3 tag" + data * 8) == {
        "frames": 8, "bitrate_kbps": 64, "sample_rate": 44100}
    assert consecutive_frames(data * 4 + b"bad frame" + data * 4) is None


def test_mpeg2_frame_size_and_invalid_headers():
    assert frame_info(b"\xff\xf3\x80\x00", 0) == (208, 64, 22050)
    assert consecutive_frames(frame(b"\xff\xf3\x80\x00") * 8)["sample_rate"] == 22050
    for header in (b"\xff\xf9\x50\x00", b"\xff\xfb\x5c\x00",
                   b"\xff\xfb\x00\x00", b"\xff\xfb\x50\x02"):
        assert frame_info(header, 0) is None


def test_candidates_filter_cities_deduplicate_and_require_live_id():
    source = ("北京新闻,http://lhttp.qingting.fm/live/339/64k.mp3\n"
              "北京新闻副本,http://lhttp.qingting.fm/live/339/64k.m3u8\n"
              "未知城市,http://lhttp.qingting.fm/live/999/64k.mp3\n"
              "上海新闻,http://example.org/audio.mp3\n")
    assert candidates(source) == [{"city": "北京", "name": "北京新闻",
        "platform_id": "339", "url": "http://lhttp.qingting.fm/live/339/64k.mp3"}]
