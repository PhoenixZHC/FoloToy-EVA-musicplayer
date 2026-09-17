#include "fam1_format.h"

#include <string.h>

#define FAM1_BLOCK_HEADER_SIZE 8u
#define FAM1_CRC_CHUNK_SIZE 512u

static const int16_t IMA_STEP_TABLE[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

static const int8_t IMA_INDEX_TABLE[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int16_t get_s16(const uint8_t *p)
{
    return (int16_t)get_u16(p);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool range_valid(uint32_t offset, uint32_t len, uint32_t end)
{
    return offset <= end && len <= end - offset;
}

uint32_t fam1_crc32_update(uint32_t state, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        state ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            state = (state >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(state & 1u));
    }
    return state;
}

static bool valid_utf8_title(const uint8_t *title, uint16_t len)
{
    uint16_t i = 0;
    while (i < len) {
        uint8_t c = title[i];
        if (c < 0x20 || c == 0x7f) return false;
        if (c < 0x80) {
            i++;
        } else if ((c & 0xe0u) == 0xc0u) {
            if (i + 1u >= len || (title[i + 1u] & 0xc0u) != 0x80u || c < 0xc2u)
                return false;
            i += 2u;
        } else if ((c & 0xf0u) == 0xe0u) {
            if (i + 2u >= len ||
                (title[i + 1u] & 0xc0u) != 0x80u ||
                (title[i + 2u] & 0xc0u) != 0x80u)
                return false;
            if (c == 0xe0u && title[i + 1u] < 0xa0u) return false;
            if (c == 0xedu && title[i + 1u] >= 0xa0u) return false;
            i += 3u;
        } else if ((c & 0xf8u) == 0xf0u) {
            if (i + 3u >= len ||
                (title[i + 1u] & 0xc0u) != 0x80u ||
                (title[i + 2u] & 0xc0u) != 0x80u ||
                (title[i + 3u] & 0xc0u) != 0x80u)
                return false;
            if (c == 0xf0u && title[i + 1u] < 0x90u) return false;
            if (c > 0xf4u || (c == 0xf4u && title[i + 1u] > 0x8fu)) return false;
            i += 4u;
        } else {
            return false;
        }
    }
    return true;
}

static bool parse_header(const fam1_reader_t *reader, uint32_t file_size,
                         fam1_header_t *header)
{
    uint8_t raw[FAM1_HEADER_FIXED_SIZE];
    if (!reader || !reader->read || !header) return false;
    if (file_size < FAM1_HEADER_FIXED_SIZE + FAM1_BLOCK_HEADER_SIZE ||
        file_size > FAM1_MAX_FILE_SIZE) return false;
    if (!reader->read(reader->ctx, 0, raw, sizeof(raw))) return false;
    if (memcmp(raw, "FAM1", 4) != 0) return false;
    if (get_u16(raw + 4) != 1) return false;

    uint16_t header_size = get_u16(raw + 6);
    uint16_t title_len = get_u16(raw + 14);
    if (title_len > FAM1_MAX_TITLE_BYTES) return false;
    if (header_size != FAM1_HEADER_FIXED_SIZE + title_len) return false;
    if (!range_valid(0, header_size, file_size)) return false;

    uint16_t sample_rate = get_u16(raw + 8);
    uint8_t channels = raw[10];
    uint8_t codec = raw[11];
    uint16_t block_samples = get_u16(raw + 12);
    uint32_t sample_count = get_u32(raw + 16);
    uint32_t duration_ms = get_u32(raw + 20);
    uint32_t payload_size = get_u32(raw + 24);
    uint32_t payload_crc32 = get_u32(raw + 28);

    if (sample_rate != 8000u && sample_rate != 12000u) return false;
    if (channels != 1u || codec != 1u || block_samples != FAM1_BLOCK_SAMPLES)
        return false;
    if (sample_count == 0) return false;
    if (duration_ms > FAM1_MAX_DURATION_MS) return false;
    if (duration_ms != (uint32_t)(((uint64_t)sample_count * 1000u + sample_rate / 2u) / sample_rate))
        return false;
    if (payload_size != file_size - header_size) return false;

    memset(header, 0, sizeof(*header));
    header->header_size = header_size;
    header->sample_rate = sample_rate;
    header->channels = channels;
    header->codec = codec;
    header->block_samples = block_samples;
    header->sample_count = sample_count;
    header->duration_ms = duration_ms;
    header->payload_size = payload_size;
    header->payload_crc32 = payload_crc32;
    header->file_size = file_size;
    header->first_block_offset = header_size;

    if (title_len > 0) {
        uint8_t title[FAM1_MAX_TITLE_BYTES];
        if (!reader->read(reader->ctx, FAM1_HEADER_FIXED_SIZE, title, title_len))
            return false;
        if (!valid_utf8_title(title, title_len)) return false;
        memcpy(header->title, title, title_len);
        header->title[title_len] = '\0';
    }
    return true;
}

bool fam1_read_header(const fam1_reader_t *reader, uint32_t file_size,
                      fam1_header_t *out)
{
    return parse_header(reader, file_size, out);
}

static bool read_block_record(const fam1_reader_t *reader, uint32_t file_size,
                              uint32_t offset, fam1_block_t *block)
{
    uint8_t raw[FAM1_BLOCK_HEADER_SIZE];
    if (!range_valid(offset, sizeof(raw), file_size)) return false;
    if (!reader->read(reader->ctx, offset, raw, sizeof(raw))) return false;

    block->predictor = get_s16(raw);
    block->step_index = raw[2];
    uint8_t flags = raw[3];
    block->sample_count = get_u16(raw + 4);
    block->encoded_size = get_u16(raw + 6);
    block->next_offset = offset + FAM1_BLOCK_HEADER_SIZE + block->encoded_size;

    if (block->step_index > 88u || flags != 0) return false;
    if (block->sample_count == 0 || block->sample_count > FAM1_BLOCK_SAMPLES)
        return false;
    if (block->encoded_size != (uint16_t)((block->sample_count - 1u + 1u) / 2u))
        return false;
    return range_valid(offset + FAM1_BLOCK_HEADER_SIZE, block->encoded_size, file_size);
}

bool fam1_validate(const fam1_reader_t *reader, uint32_t file_size, fam1_header_t *out)
{
    fam1_header_t header;
    if (!parse_header(reader, file_size, &header)) return false;

    uint8_t chunk[FAM1_CRC_CHUNK_SIZE];
    uint32_t state = 0xffffffffu;
    uint32_t offset = header.first_block_offset;
    while (offset < file_size) {
        uint32_t len = file_size - offset;
        if (len > sizeof(chunk)) len = sizeof(chunk);
        if (!reader->read(reader->ctx, offset, chunk, len)) return false;
        state = fam1_crc32_update(state, chunk, len);
        offset += len;
    }
    if ((state ^ 0xffffffffu) != header.payload_crc32) return false;

    offset = header.first_block_offset;
    uint32_t samples = 0;
    while (offset < file_size) {
        fam1_block_t block;
        if (!read_block_record(reader, file_size, offset, &block)) return false;
        if (block.sample_count > header.sample_count - samples) return false;
        samples += block.sample_count;
        if (samples < header.sample_count && block.sample_count != FAM1_BLOCK_SAMPLES)
            return false;
        offset = block.next_offset;
    }
    if (offset != file_size || samples != header.sample_count) return false;

    if (out) *out = header;
    return true;
}

static int16_t clamp_i16(int32_t value)
{
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return (int16_t)value;
}

bool fam1_decode_block(const uint8_t *block_bytes, size_t block_len,
                       int16_t *pcm, size_t pcm_capacity, fam1_block_t *out)
{
    if (!block_bytes || !pcm || !out || block_len < FAM1_BLOCK_HEADER_SIZE)
        return false;

    fam1_block_t block;
    block.predictor = get_s16(block_bytes);
    block.step_index = block_bytes[2];
    uint8_t flags = block_bytes[3];
    block.sample_count = get_u16(block_bytes + 4);
    block.encoded_size = get_u16(block_bytes + 6);
    block.next_offset = FAM1_BLOCK_HEADER_SIZE + block.encoded_size;

    if (block.step_index > 88u || flags != 0) return false;
    if (block.sample_count == 0 || block.sample_count > FAM1_BLOCK_SAMPLES)
        return false;
    if (block.sample_count > pcm_capacity) return false;
    if (block.encoded_size != (uint16_t)((block.sample_count - 1u + 1u) / 2u))
        return false;
    if (block.next_offset > block_len) return false;

    int32_t predictor = block.predictor;
    uint8_t step_index = block.step_index;
    pcm[0] = (int16_t)predictor;
    for (uint16_t i = 1; i < block.sample_count; i++) {
        uint8_t byte = block_bytes[FAM1_BLOCK_HEADER_SIZE + ((i - 1u) >> 1)];
        uint8_t nibble = ((i - 1u) & 1u) ? (byte >> 4) : (byte & 0x0fu);
        int32_t step = IMA_STEP_TABLE[step_index];
        int32_t diff = step >> 3;
        if (nibble & 1u) diff += step >> 2;
        if (nibble & 2u) diff += step >> 1;
        if (nibble & 4u) diff += step;
        if (nibble & 8u) predictor -= diff;
        else predictor += diff;
        predictor = clamp_i16(predictor);

        int next_index = (int)step_index + IMA_INDEX_TABLE[nibble];
        if (next_index < 0) next_index = 0;
        if (next_index > 88) next_index = 88;
        step_index = (uint8_t)next_index;
        pcm[i] = (int16_t)predictor;
    }

    *out = block;
    return true;
}
