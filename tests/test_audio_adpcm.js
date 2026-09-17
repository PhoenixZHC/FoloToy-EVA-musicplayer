const assert = require('assert');

const {
  encodePcm16ToFam1,
  estimateFam1Bytes,
  sanitizeAudioTitle,
} = require('../main/audio_adpcm.js');

function viewOf(bytes) {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
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

function testHeaderAndPayloadCrc() {
  const pcm = Int16Array.from([0, 1000, -1000, 2000, -2000]);
  const bytes = encodePcm16ToFam1(pcm, 12000, 'demo.mp3');
  const view = viewOf(bytes);

  assert.strictEqual(Buffer.from(bytes.subarray(0, 4)).toString('ascii'), 'FAM1');
  assert.strictEqual(view.getUint16(4, true), 1);
  assert.strictEqual(view.getUint16(6, true), 36);
  assert.strictEqual(view.getUint16(8, true), 12000);
  assert.strictEqual(view.getUint8(10), 1);
  assert.strictEqual(view.getUint8(11), 1);
  assert.strictEqual(view.getUint16(12, true), 512);
  assert.strictEqual(view.getUint16(14, true), 4);
  assert.strictEqual(view.getUint32(16, true), pcm.length);
  assert.strictEqual(view.getUint32(20, true), 0);
  assert.strictEqual(view.getUint32(24, true), bytes.length - 36);
  assert.strictEqual(view.getUint32(28, true), crc32(bytes.subarray(36)));
  assert.strictEqual(Buffer.from(bytes.subarray(32, 36)).toString('utf8'), 'demo');
  assert.strictEqual(bytes.length, estimateFam1Bytes(pcm.length, 'demo'));
}

function testBlockLayoutAndLowNibbleFirst() {
  const pcm = Int16Array.from([1000, 1001, 1012]);
  const bytes = encodePcm16ToFam1(pcm, 8000, 'tone');
  const view = viewOf(bytes);
  const blockOffset = view.getUint16(6, true);

  assert.strictEqual(view.getInt16(blockOffset + 0, true), 1000);
  assert.strictEqual(view.getUint8(blockOffset + 2), 0);
  assert.strictEqual(view.getUint8(blockOffset + 3), 0);
  assert.strictEqual(view.getUint16(blockOffset + 4, true), 3);
  assert.strictEqual(view.getUint16(blockOffset + 6, true), 1);
  assert.strictEqual(bytes[blockOffset + 8], 0x71);
}

function testMultiBlockSizeAndDuration() {
  const pcm = new Int16Array(513);
  pcm[0] = 12;
  pcm[512] = -12;
  const bytes = encodePcm16ToFam1(pcm, 12000, 'two-blocks');
  const view = viewOf(bytes);
  const blockOffset = view.getUint16(6, true);
  const firstSize = 8 + view.getUint16(blockOffset + 6, true);
  const secondOffset = blockOffset + firstSize;

  assert.strictEqual(view.getUint32(16, true), 513);
  assert.strictEqual(view.getUint32(20, true), 43);
  assert.strictEqual(view.getUint16(blockOffset + 4, true), 512);
  assert.strictEqual(view.getUint16(secondOffset + 4, true), 1);
  assert.strictEqual(view.getUint16(secondOffset + 6, true), 0);
}

function testTitleSanitizerAndValidation() {
  assert.strictEqual(sanitizeAudioTitle('  a\0b.mp3  '), 'ab');
  assert.strictEqual(sanitizeAudioTitle(''), '音乐');
  assert.strictEqual(Buffer.from(sanitizeAudioTitle('长'.repeat(100))).length <= 64, true);
  assert.throws(() => encodePcm16ToFam1(new Int16Array(0), 12000, 'empty'), /sample/i);
  assert.throws(() => encodePcm16ToFam1(Int16Array.from([0]), 11025, 'bad'), /sample rate/i);
}

function testMultiDotFilenameFitsEncodedBuffer() {
  const pcm = new Int16Array(512);
  const bytes = encodePcm16ToFam1(pcm, 12000, 'one.last.kiss.mp3');
  assert.strictEqual(bytes.length, estimateFam1Bytes(pcm.length, 'one.last.kiss.mp3'));
  assert.strictEqual(Buffer.from(bytes.subarray(32, 45)).toString('utf8'), 'one.last.kiss');
}

testHeaderAndPayloadCrc();
testBlockLayoutAndLowNibbleFirst();
testMultiBlockSizeAndDuration();
testTitleSanitizerAndValidation();
testMultiDotFilenameFitsEncodedBuffer();
console.log('audio adpcm tests passed');
