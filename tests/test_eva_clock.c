#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "eva_clock.h"

static void test_splits_elapsed_time_into_display_digits(void)
{
    uint8_t digits[EVA_CLOCK_DIGIT_COUNT] = { 0 };

    eva_clock_digits(42318, digits);

    const uint8_t expected[EVA_CLOCK_DIGIT_COUNT] = { 0, 0, 4, 2, 3, 1, 8 };
    for (int i = 0; i < EVA_CLOCK_DIGIT_COUNT; i++) {
        assert(digits[i] == expected[i]);
    }
}

static void test_renders_without_writing_past_clock_width(void)
{
    enum { STRIDE = EVA_CLOCK_WIDTH + 2 };
    uint8_t buffer[EVA_CLOCK_HEIGHT * STRIDE];
    memset(buffer, 0xA5, sizeof(buffer));

    assert(eva_clock_render_a8(buffer, STRIDE, 42318));

    bool has_lit_pixel = false;
    for (int y = 0; y < EVA_CLOCK_HEIGHT; y++) {
        for (int x = 0; x < EVA_CLOCK_WIDTH; x++) {
            if (buffer[y * STRIDE + x] == 0xFF) has_lit_pixel = true;
        }
        assert(buffer[y * STRIDE + EVA_CLOCK_WIDTH] == 0xA5);
        assert(buffer[y * STRIDE + EVA_CLOCK_WIDTH + 1] == 0xA5);
    }
    assert(has_lit_pixel);
}

int main(void)
{
    test_splits_elapsed_time_into_display_digits();
    test_renders_without_writing_past_clock_width();
    return 0;
}
