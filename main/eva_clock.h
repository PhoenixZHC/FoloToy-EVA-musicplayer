#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVA_CLOCK_DIGIT_COUNT 7
#define EVA_CLOCK_WIDTH 224
#define EVA_CLOCK_HEIGHT 82

void eva_clock_digits(uint32_t elapsed_ms, uint8_t digits[EVA_CLOCK_DIGIT_COUNT]);
bool eva_clock_render_a8(uint8_t *buffer, size_t stride, uint32_t elapsed_ms);
