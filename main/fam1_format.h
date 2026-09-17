#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FAM1_HEADER_FIXED_SIZE 32u
#define FAM1_BLOCK_SAMPLES 512u
#define FAM1_MAX_FILE_SIZE (2560u * 1024u)
#define FAM1_MAX_DURATION_MS (6u * 60u * 1000u)
#define FAM1_MAX_TITLE_BYTES 64u

typedef bool (*fam1_read_fn_t)(void *ctx, uint32_t offset, void *out, size_t len);

typedef struct {
    fam1_read_fn_t read;
    void *ctx;
} fam1_reader_t;

typedef struct {
    uint16_t header_size;
    uint16_t sample_rate;
    uint8_t channels;
    uint8_t codec;
    uint16_t block_samples;
    uint32_t sample_count;
    uint32_t duration_ms;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t file_size;
    uint32_t first_block_offset;
    char title[FAM1_MAX_TITLE_BYTES + 1u];
} fam1_header_t;

typedef struct {
    int16_t predictor;
    uint8_t step_index;
    uint16_t sample_count;
    uint16_t encoded_size;
    uint32_t next_offset;
} fam1_block_t;

uint32_t fam1_crc32_update(uint32_t state, const uint8_t *data, size_t len);
bool fam1_read_header(const fam1_reader_t *reader, uint32_t file_size,
                      fam1_header_t *out);
bool fam1_validate(const fam1_reader_t *reader, uint32_t file_size, fam1_header_t *out);
bool fam1_decode_block(const uint8_t *block, size_t block_len,
                       int16_t *pcm, size_t pcm_capacity, fam1_block_t *out);
