import argparse
import struct
import wave
from pathlib import Path

INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]
STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
    598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
    2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
    18500, 20350, 22385, 24623, 27086, 29794, 32767,
]


def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def encode_sample(sample, predictor, index):
    step = STEP_TABLE[index]
    diff = sample - predictor
    nibble = 0
    if diff < 0:
        nibble = 8
        diff = -diff

    delta = step >> 3
    if diff >= step:
        nibble |= 4
        diff -= step
        delta += step
    if diff >= step >> 1:
        nibble |= 2
        diff -= step >> 1
        delta += step >> 1
    if diff >= step >> 2:
        nibble |= 1
        delta += step >> 2

    if nibble & 8:
        predictor -= delta
    else:
        predictor += delta
    predictor = clamp(predictor, -32768, 32767)
    index = clamp(index + INDEX_TABLE[nibble], 0, 88)
    return nibble & 0xF, predictor, index


def read_wav(path):
    with wave.open(str(path), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            raise SystemExit("Input WAV must be 16-bit mono PCM")
        sample_rate = wav.getframerate()
        raw = wav.readframes(wav.getnframes())
    samples = struct.unpack("<" + "h" * (len(raw) // 2), raw)
    return sample_rate, samples


def write_adpcm(path, sample_rate, samples):
    if not samples:
        raise SystemExit("No samples")
    predictor = int(samples[0])
    index = 0
    nibbles = []
    for sample in samples[1:]:
        nibble, predictor, index = encode_sample(int(sample), predictor, index)
        nibbles.append(nibble)

    payload = bytearray()
    for i in range(0, len(nibbles), 2):
        lo = nibbles[i]
        hi = nibbles[i + 1] if i + 1 < len(nibbles) else 0
        payload.append(lo | (hi << 4))

    header = struct.pack("<4sHhBBHI", b"EVA1", sample_rate, int(samples[0]), 0, 0, 0, len(samples))
    Path(path).write_bytes(header + payload)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_wav")
    parser.add_argument("output_adpcm")
    args = parser.parse_args()
    sample_rate, samples = read_wav(args.input_wav)
    write_adpcm(args.output_adpcm, sample_rate, samples)


if __name__ == "__main__":
    main()
