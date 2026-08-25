#include <assert.h>
#include <stdint.h>
#include "eva_adpcm.h"

static void put_le32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static void test_rejects_truncated_payload(void)
{
    uint8_t data[17] = { 'E', 'V', 'A', '1', 0x40, 0x1F };
    put_le32(data + 12, 10);

    eva_adpcm_decoder_t decoder;
    assert(!eva_adpcm_reset(&decoder, data, data + sizeof(data), 8000));
}

static void test_accepts_exact_payload(void)
{
    uint8_t data[18] = { 'E', 'V', 'A', '1', 0x40, 0x1F };
    put_le32(data + 12, 5);

    eva_adpcm_decoder_t decoder;
    assert(eva_adpcm_reset(&decoder, data, data + sizeof(data), 8000));
    assert(eva_adpcm_sample_count(&decoder) == 5);
}

int main(void)
{
    test_rejects_truncated_payload();
    test_accepts_exact_payload();
    return 0;
}
