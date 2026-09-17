(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.FoloAudio = factory();
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  const HEADER_SIZE = 32;
  const BLOCK_SAMPLES = 512;
  const MAX_FAM1_BYTES = 2560 * 1024;
  const MAX_TITLE_BYTES = 64;
  const VALID_SAMPLE_RATES = new Set([8000, 12000]);

  const STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
  ];

  const INDEX_TABLE = [
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
  ];

  function utf8Bytes(text) {
    return new TextEncoder().encode(text);
  }

  function clampI16(value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return value | 0;
  }

  function crc32(bytes) {
    let crc = 0xffffffff;
    for (const byte of bytes) {
      crc ^= byte;
      for (let bit = 0; bit < 8; bit++) {
        const mask = -(crc & 1);
        crc = (crc >>> 1) ^ (0xedb88320 & mask);
      }
    }
    return (crc ^ 0xffffffff) >>> 0;
  }

  function truncateUtf8(text, maxBytes) {
    let out = '';
    let used = 0;
    for (const char of text) {
      const len = utf8Bytes(char).length;
      if (used + len > maxBytes) break;
      out += char;
      used += len;
    }
    return out;
  }

  function sanitizeAudioTitle(name) {
    let title = String(name || '').trim();
    title = title.replace(/\.[^.\\/]+$/u, '');
    title = title.replace(/[\u0000-\u001f\u007f]/gu, '').trim();
    if (!title) title = '音乐';
    return truncateUtf8(title, MAX_TITLE_BYTES);
  }

  function estimateFam1Bytes(sampleCount, title = '') {
    if (!Number.isFinite(sampleCount) || sampleCount < 1) {
      throw new Error('sample count must be positive');
    }
    const safeTitle = title ? sanitizeAudioTitle(title) : '';
    let size = HEADER_SIZE + utf8Bytes(safeTitle).length;
    let remaining = sampleCount;
    while (remaining > 0) {
      const count = Math.min(BLOCK_SAMPLES, remaining);
      size += 8 + Math.ceil((count - 1) / 2);
      remaining -= count;
    }
    return size;
  }

  function encodeNibble(sample, predictor, stepIndex) {
    let step = STEP_TABLE[stepIndex];
    let diff = sample - predictor;
    let nibble = 0;
    if (diff < 0) {
      nibble = 8;
      diff = -diff;
    }

    let delta = step >> 3;
    if (diff >= step) {
      nibble |= 4;
      diff -= step;
      delta += step;
    }
    step >>= 1;
    if (diff >= step) {
      nibble |= 2;
      diff -= step;
      delta += step;
    }
    step >>= 1;
    if (diff >= step) {
      nibble |= 1;
      delta += step;
    }

    if (nibble & 8) predictor -= delta;
    else predictor += delta;
    predictor = clampI16(predictor);

    stepIndex += INDEX_TABLE[nibble & 0x0f];
    if (stepIndex < 0) stepIndex = 0;
    if (stepIndex > 88) stepIndex = 88;
    return { nibble: nibble & 0x0f, predictor, stepIndex };
  }

  function encodeBlock(pcm, start, count, stepIndex) {
    const predictor = clampI16(pcm[start] | 0);
    const bytes = new Uint8Array(8 + Math.ceil((count - 1) / 2));
    const view = new DataView(bytes.buffer);
    view.setInt16(0, predictor, true);
    view.setUint8(2, stepIndex);
    view.setUint8(3, 0);
    view.setUint16(4, count, true);
    view.setUint16(6, bytes.length - 8, true);

    let pred = predictor;
    for (let i = 1; i < count; i++) {
      const code = encodeNibble(clampI16(pcm[start + i] | 0), pred, stepIndex);
      pred = code.predictor;
      stepIndex = code.stepIndex;
      const encodedIndex = 8 + ((i - 1) >> 1);
      if (((i - 1) & 1) === 0) bytes[encodedIndex] = code.nibble;
      else bytes[encodedIndex] |= code.nibble << 4;
    }
    return { bytes, nextStepIndex: stepIndex };
  }

  function encodePcm16ToFam1(pcm16, sampleRate, title) {
    if (!(pcm16 instanceof Int16Array)) throw new Error('pcm16 must be Int16Array');
    if (pcm16.length < 1) throw new Error('sample count must be positive');
    if (!VALID_SAMPLE_RATES.has(sampleRate)) throw new Error('unsupported sample rate');

    const cleanTitle = sanitizeAudioTitle(title);
    const titleBytes = utf8Bytes(cleanTitle);
    const totalSize = estimateFam1Bytes(pcm16.length, title);
    if (totalSize > MAX_FAM1_BYTES) throw new Error(`FAM1 file exceeds ${MAX_FAM1_BYTES} bytes`);

    const bytes = new Uint8Array(totalSize);
    const view = new DataView(bytes.buffer);
    bytes.set([0x46, 0x41, 0x4d, 0x31], 0);
    view.setUint16(4, 1, true);
    view.setUint16(6, HEADER_SIZE + titleBytes.length, true);
    view.setUint16(8, sampleRate, true);
    view.setUint8(10, 1);
    view.setUint8(11, 1);
    view.setUint16(12, BLOCK_SAMPLES, true);
    view.setUint16(14, titleBytes.length, true);
    view.setUint32(16, pcm16.length, true);
    view.setUint32(20, Math.floor((pcm16.length * 1000 + sampleRate / 2) / sampleRate), true);
    bytes.set(titleBytes, HEADER_SIZE);

    let offset = HEADER_SIZE + titleBytes.length;
    let start = 0;
    let stepIndex = 0;
    while (start < pcm16.length) {
      const count = Math.min(BLOCK_SAMPLES, pcm16.length - start);
      const block = encodeBlock(pcm16, start, count, stepIndex);
      bytes.set(block.bytes, offset);
      offset += block.bytes.length;
      start += count;
      stepIndex = block.nextStepIndex;
    }

    view.setUint32(24, bytes.length - (HEADER_SIZE + titleBytes.length), true);
    view.setUint32(28, crc32(bytes.subarray(HEADER_SIZE + titleBytes.length)), true);
    return bytes;
  }

  return {
    HEADER_SIZE,
    BLOCK_SAMPLES,
    MAX_FAM1_BYTES,
    sanitizeAudioTitle,
    estimateFam1Bytes,
    encodePcm16ToFam1,
  };
});
