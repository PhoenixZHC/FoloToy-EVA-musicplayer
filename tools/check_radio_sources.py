"""检查公开目录的城市候选：HTTP 类型与连续 MP3 帧均通过才标记可收录。"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import time
import urllib.request

CITY_NAMES = (
    "北京", "上海", "广州", "深圳", "杭州", "南京", "天津", "重庆", "成都", "武汉",
    "西安", "长沙", "郑州", "苏州", "宁波", "无锡", "佛山", "东莞", "青岛", "济南",
    "合肥", "厦门", "沈阳",
)


def frame_info(data, offset):
    if len(data) - offset < 4:
        return None
    a, b, c, d = data[offset:offset + 4]
    version, layer, rate, bitrate = (b >> 3) & 3, (b >> 1) & 3, (c >> 2) & 3, c >> 4
    if a != 255 or b & 224 != 224 or version == 1 or layer != 1 or rate == 3 or bitrate in (0, 15) or d & 3 == 2:
        return None
    hz = (44100, 48000, 32000)[rate] // (1 if version == 3 else 2 if version == 2 else 4)
    rates = (0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320) if version == 3 else (0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160)
    kbps = rates[bitrate]
    size = (144000 if version == 3 else 72000) * kbps // hz + ((c >> 1) & 1)
    return size, kbps, hz


def consecutive_frames(data):
    for start in range(max(0, len(data) - 3)):
        position, count, info = start, 0, None
        while True:
            current = frame_info(data, position)
            if not current or position + current[0] > len(data):
                break
            if info and current[1:] != info[1:]:
                break
            info = current
            count += 1
            position += current[0]
            if count >= 8:
                return {"frames": count, "bitrate_kbps": info[1], "sample_rate": info[2]}
    return None


def candidates(source):
    found = {}
    for line in source.splitlines():
        if "," not in line:
            continue
        name, url = line.split(",", 1)
        match = re.search(r"qingting[.]fm/live/(\d+)", url)
        city = next((city for city in CITY_NAMES if city in name), None)
        if not match or not city:
            continue
        identifier = match.group(1)
        # HLS 目录里的编号仅作为候选；替代 MP3 地址必须重新取样验证。
        key = city, identifier
        found.setdefault(key, {"city": city, "name": name.strip(), "platform_id": identifier,
            "url": f"http://lhttp.qingting.fm/live/{identifier}/64k.mp3"})
    return list(found.values())


def check(candidate):
    result = dict(candidate, ok=False)
    started = time.monotonic()
    request = urllib.request.Request(candidate["url"], headers={"User-Agent": "EVA-Radio-Check/1.0", "Icy-MetaData": "0"})
    try:
        with urllib.request.urlopen(request, timeout=4) as response:
            content_type = response.headers.get("Content-Type", "").lower()
            result.update(status=response.status, content_type=content_type)
            if response.status != 200 or not content_type.startswith(("audio/mpeg", "audio/mp3")):
                raise ValueError("device-incompatible response")
            if int(response.headers.get("icy-metaint", "0")):
                raise ValueError("metadata present despite disabled request")
            data = bytearray()
            while len(data) < 8192 and time.monotonic() - started < 10:
                part = response.read1(min(2048, 8192 - len(data)))
                if not part:
                    break
                data.extend(part)
                info = consecutive_frames(data)
                if info:
                    if not 32 <= info["bitrate_kbps"] <= 128:
                        raise ValueError("bitrate outside device catalog policy")
                    result.update(info, ok=True, sampled_bytes=len(data))
                    break
            if not result["ok"]:
                raise ValueError("eight consecutive MP3 frames not found within sample limit")
    except Exception as error:
        result["error"] = f"{type(error).__name__}: {error}"
    result["elapsed_seconds"] = round(time.monotonic() - started, 2)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=8, choices=range(1, 9))
    args = parser.parse_args()
    rows = candidates(args.source.read_text(encoding="utf-8-sig"))
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        results = list(pool.map(check, rows))
    args.output.write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
    good = [row for row in results if row["ok"]]
    print(json.dumps({"checked": len(results), "passed": len(good), "failed": len(results) - len(good),
        "cities": {city: sum(row["city"] == city for row in good) for city in CITY_NAMES if any(row["city"] == city for row in good)}}, ensure_ascii=False))


if __name__ == "__main__":
    main()
