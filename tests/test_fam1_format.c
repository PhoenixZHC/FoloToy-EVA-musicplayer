#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fam1_format.h"

typedef struct {
    uint8_t *data;
    uint32_t size;
} memory_file_t;

static bool memory_read(void *ctx, uint32_t offset, void *out, size_t len)
{
    memory_file_t *file = ctx;
    if (!file || !out || (uint64_t)offset + len > file->size) return false;
    memcpy(out, file->data + offset, len);
    return true;
}

static void put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t fixture_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return crc ^ 0xffffffffu;
}

static memory_file_t make_valid_fam1_file(const char *title, uint16_t sample_rate,
                                          uint32_t sample_count)
{
    const uint16_t title_len = (uint16_t)strlen(title);
    const uint16_t block_samples = (uint16_t)sample_count;
    const uint16_t encoded_size = (uint16_t)((block_samples - 1u + 1u) / 2u);
    const uint32_t payload_size = 8u + encoded_size;
    memory_file_t file = {
        .size = FAM1_HEADER_FIXED_SIZE + title_len + payload_size,
        .data = calloc(1, FAM1_HEADER_FIXED_SIZE + title_len + payload_size),
    };
    assert(file.data != NULL);

    memcpy(file.data, "FAM1", 4);
    put_u16(file.data + 4, 1);
    put_u16(file.data + 6, FAM1_HEADER_FIXED_SIZE + title_len);
    put_u16(file.data + 8, sample_rate);
    file.data[10] = 1;
    file.data[11] = 1;
    put_u16(file.data + 12, FAM1_BLOCK_SAMPLES);
    put_u16(file.data + 14, title_len);
    put_u32(file.data + 16, sample_count);
    put_u32(file.data + 20,
            (uint32_t)(((uint64_t)sample_count * 1000u + sample_rate / 2u) / sample_rate));
    put_u32(file.data + 24, payload_size);
    memcpy(file.data + FAM1_HEADER_FIXED_SIZE, title, title_len);

    uint8_t *block = file.data + FAM1_HEADER_FIXED_SIZE + title_len;
    put_u16(block + 0, 0);
    block[2] = 0;
    block[3] = 0;
    put_u16(block + 4, block_samples);
    put_u16(block + 6, encoded_size);

    put_u32(file.data + 28, fixture_crc32(block, payload_size));
    return file;
}

static void refresh_crc(memory_file_t *file)
{
    uint16_t title_len = (uint16_t)file->data[14] | ((uint16_t)file->data[15] << 8);
    uint16_t header_size = FAM1_HEADER_FIXED_SIZE + title_len;
    put_u32(file->data + 28, fixture_crc32(file->data + header_size,
                                           file->size - header_size));
}

static void test_valid_single_block_file(void)
{
    memory_file_t file = make_valid_fam1_file("Song", 12000, 8);
    fam1_reader_t reader = { .read = memory_read, .ctx = &file };
    fam1_header_t header;
    assert(fam1_read_header(&reader, file.size, &header));
    assert(fam1_validate(&reader, file.size, &header));
    assert(header.sample_rate == 12000);
    assert(header.channels == 1);
    assert(header.block_samples == 512);
    assert(header.sample_count == 8);
    assert(header.duration_ms == 1);
    assert(header.payload_size == 12);
    assert(header.header_size == 36);
    assert(strcmp(header.title, "Song") == 0);

    int16_t pcm[512];
    fam1_block_t block;
    assert(fam1_decode_block(file.data + header.header_size,
                             file.size - header.header_size,
                             pcm, 512, &block));
    assert(block.sample_count == 8);
    assert(block.encoded_size == 4);
    assert(block.next_offset == 12);
    for (uint32_t i = 0; i < block.sample_count; i++)
        assert(pcm[i] == 0);
    free(file.data);
}

static void test_header_read_does_not_scan_payload(void)
{
    memory_file_t file = make_valid_fam1_file("Song", 12000, 8);
    fam1_reader_t reader = { .read = memory_read, .ctx = &file };
    file.data[file.size - 1u] ^= 1u;

    fam1_header_t header;
    assert(fam1_read_header(&reader, file.size, &header));
    assert(header.sample_rate == 12000);
    assert(!fam1_validate(&reader, file.size, NULL));
    free(file.data);
}

static void test_decode_block_uses_low_nibble_first(void)
{
    uint8_t raw[] = {
        0xe8, 0x03, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x71,
    };
    int16_t pcm[3] = {0};
    fam1_block_t block;
    assert(fam1_decode_block(raw, sizeof(raw), pcm, 3, &block));
    assert(block.predictor == 1000);
    assert(block.sample_count == 3);
    assert(block.next_offset == sizeof(raw));
    assert(pcm[0] == 1000);
    assert(pcm[1] == 1001);
    assert(pcm[2] == 1012);
}

static void test_invalid_magic_crc_and_duration_are_rejected(void)
{
    memory_file_t file = make_valid_fam1_file("Song", 8000, 8);
    fam1_reader_t reader = { .read = memory_read, .ctx = &file };

    file.data[0] = 'X';
    assert(!fam1_validate(&reader, file.size, NULL));
    file.data[0] = 'F';

    uint16_t title_len = (uint16_t)file.data[14] | ((uint16_t)file.data[15] << 8);
    file.data[FAM1_HEADER_FIXED_SIZE + title_len + 8] ^= 1;
    assert(!fam1_validate(&reader, file.size, NULL));
    file.data[FAM1_HEADER_FIXED_SIZE + title_len + 8] ^= 1;

    put_u32(file.data + 20, 99);
    refresh_crc(&file);
    assert(!fam1_validate(&reader, file.size, NULL));
    free(file.data);
}

static void test_bad_block_boundaries_are_rejected(void)
{
    memory_file_t file = make_valid_fam1_file("Song", 12000, 8);
    fam1_reader_t reader = { .read = memory_read, .ctx = &file };
    uint8_t *block = file.data + FAM1_HEADER_FIXED_SIZE + 4;

    block[2] = 89;
    refresh_crc(&file);
    assert(!fam1_validate(&reader, file.size, NULL));
    block[2] = 0;

    put_u16(block + 6, 99);
    refresh_crc(&file);
    assert(!fam1_validate(&reader, file.size, NULL));
    free(file.data);
}

static void test_control_characters_in_title_are_rejected(void)
{
    memory_file_t file = make_valid_fam1_file("Bad\nTitle", 12000, 8);
    fam1_reader_t reader = { .read = memory_read, .ctx = &file };
    assert(!fam1_validate(&reader, file.size, NULL));
    free(file.data);
}

int main(void)
{
    test_valid_single_block_file();
    test_header_read_does_not_scan_payload();
    test_decode_block_uses_low_nibble_first();
    test_invalid_magic_crc_and_duration_are_rejected();
    test_bad_block_boundaries_are_rejected();
    test_control_characters_in_title_are_rejected();
    puts("fam1 format tests passed");
    return 0;
}
