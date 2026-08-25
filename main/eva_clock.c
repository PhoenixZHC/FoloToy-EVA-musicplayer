#include "eva_clock.h"

#include <string.h>

static void fill_rect(uint8_t *buffer, size_t stride, int x, int y, int w, int h)
{
    for (int row = y; row < y + h; row++) {
        memset(buffer + (size_t)row * stride + x, 0xFF, (size_t)w);
    }
}

static void draw_digit(uint8_t *buffer, size_t stride, int x, int y,
                       int w, int h, int t, int digit)
{
    static const uint8_t map[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
    };
    uint8_t mask = map[digit % 10];
    int half = h / 2;
    if (mask & 0x01) fill_rect(buffer, stride, x + t, y, w - 2 * t, t);
    if (mask & 0x02) fill_rect(buffer, stride, x + w - t, y + t, t, half - t);
    if (mask & 0x04) fill_rect(buffer, stride, x + w - t, y + half, t, half - t);
    if (mask & 0x08) fill_rect(buffer, stride, x + t, y + h - t, w - 2 * t, t);
    if (mask & 0x10) fill_rect(buffer, stride, x, y + half, t, half - t);
    if (mask & 0x20) fill_rect(buffer, stride, x, y + t, t, half - t);
    if (mask & 0x40) fill_rect(buffer, stride, x + t, y + half - t / 2, w - 2 * t, t);
}

void eva_clock_digits(uint32_t elapsed_ms, uint8_t digits[EVA_CLOCK_DIGIT_COUNT])
{
    uint32_t total_sec = elapsed_ms / 1000U;
    uint32_t minutes = (total_sec / 60U) % 100U;
    uint32_t seconds = total_sec % 60U;
    uint32_t millis = elapsed_ms % 1000U;

    digits[0] = (uint8_t)(minutes / 10U);
    digits[1] = (uint8_t)(minutes % 10U);
    digits[2] = (uint8_t)(seconds / 10U);
    digits[3] = (uint8_t)(seconds % 10U);
    digits[4] = (uint8_t)(millis / 100U);
    digits[5] = (uint8_t)((millis / 10U) % 10U);
    digits[6] = (uint8_t)(millis % 10U);
}

bool eva_clock_render_a8(uint8_t *buffer, size_t stride, uint32_t elapsed_ms)
{
    if (!buffer || stride < EVA_CLOCK_WIDTH) return false;
    for (int y = 0; y < EVA_CLOCK_HEIGHT; y++) {
        memset(buffer + (size_t)y * stride, 0, EVA_CLOCK_WIDTH);
    }

    uint8_t digits[EVA_CLOCK_DIGIT_COUNT];
    eva_clock_digits(elapsed_ms, digits);

    int x = 10;
    int y = 8;
    int w = 31;
    int h = 62;
    int t = 7;
    draw_digit(buffer, stride, x, y, w, h, t, digits[0]); x += 37;
    draw_digit(buffer, stride, x, y, w, h, t, digits[1]); x += 40;
    fill_rect(buffer, stride, x, y + 18, 6, 6);
    fill_rect(buffer, stride, x, y + 44, 6, 6);
    x += 14;
    draw_digit(buffer, stride, x, y, w, h, t, digits[2]); x += 37;
    draw_digit(buffer, stride, x, y, w, h, t, digits[3]);

    fill_rect(buffer, stride, 176, 57, 4, 4);
    draw_digit(buffer, stride, 184, 43, 12, 26, 3, digits[4]);
    draw_digit(buffer, stride, 198, 43, 12, 26, 3, digits[5]);
    draw_digit(buffer, stride, 212, 43, 12, 26, 3, digits[6]);
    return true;
}
