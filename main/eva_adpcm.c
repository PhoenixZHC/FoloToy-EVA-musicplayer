#include "eva_adpcm.h"

static const int INDEX_TABLE[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8,
};

static const int STEP_TABLE[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
    598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
    2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
    18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int clamp_int(int value, int min, int max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

bool eva_adpcm_reset(eva_adpcm_decoder_t *decoder, const uint8_t *start,
                     const uint8_t *end, uint16_t expected_sample_rate)
{
    if (!decoder || !start || !end || end < start || (size_t)(end - start) < 16U) {
        return false;
    }
    if (start[0] != 'E' || start[1] != 'V' || start[2] != 'A' || start[3] != '1') {
        return false;
    }
    if (read_le16(start + 4) != expected_sample_rate) {
        return false;
    }

    uint32_t samples = read_le32(start + 12);
    if (samples == 0) {
        return false;
    }
    size_t nibble_count = (size_t)samples - 1U;
    size_t payload_bytes = (nibble_count + 1U) / 2U;
    if ((size_t)(end - start) - 16U < payload_bytes) {
        return false;
    }

    decoder->nibbles = start + 16;
    decoder->nibble_count = nibble_count;
    decoder->nibble_pos = 0;
    decoder->predictor = (int)(int16_t)read_le16(start + 6);
    decoder->index = clamp_int(start[8], 0, 88);
    return true;
}

bool eva_adpcm_finished(const eva_adpcm_decoder_t *decoder)
{
    return decoder->nibble_pos >= decoder->nibble_count;
}

uint32_t eva_adpcm_sample_count(const eva_adpcm_decoder_t *decoder)
{
    return (uint32_t)(decoder->nibble_count + 1U);
}

int16_t eva_adpcm_next(eva_adpcm_decoder_t *decoder)
{
    if (eva_adpcm_finished(decoder)) {
        return (int16_t)decoder->predictor;
    }

    uint8_t packed = decoder->nibbles[decoder->nibble_pos / 2U];
    uint8_t nibble = (decoder->nibble_pos & 1U) ? (packed >> 4) : (packed & 0x0F);
    decoder->nibble_pos++;

    int step = STEP_TABLE[decoder->index];
    int diff = step >> 3;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;
    if (nibble & 8) decoder->predictor -= diff;
    else decoder->predictor += diff;

    decoder->predictor = clamp_int(decoder->predictor, -32768, 32767);
    decoder->index = clamp_int(decoder->index + INDEX_TABLE[nibble], 0, 88);
    return (int16_t)decoder->predictor;
}
