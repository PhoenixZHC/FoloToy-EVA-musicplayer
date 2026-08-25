#pragma once

#include <stdbool.h>

bool eva_track_scroll_bounds(int image_width, int viewport_width, int margin,
                             int *start_x, int *end_x);
void eva_track_marquee_positions(int image_width, int gap, int margin, int offset,
                                  int *first_x, int *second_x);
