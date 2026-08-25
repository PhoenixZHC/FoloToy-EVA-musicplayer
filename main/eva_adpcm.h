#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *nibbles;
    size_t nibble_count;
    size_t nibble_pos;
    int predictor;
    int index;
} eva_adpcm_decoder_t;

bool eva_adpcm_reset(eva_adpcm_decoder_t *decoder, const uint8_t *start,
                     const uint8_t *end, uint16_t expected_sample_rate);
bool eva_adpcm_finished(const eva_adpcm_decoder_t *decoder);
uint32_t eva_adpcm_sample_count(const eva_adpcm_decoder_t *decoder);
int16_t eva_adpcm_next(eva_adpcm_decoder_t *decoder);
