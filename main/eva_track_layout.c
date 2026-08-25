#include "eva_track_layout.h"

bool eva_track_scroll_bounds(int image_width, int viewport_width, int margin,
                             int *start_x, int *end_x)
{
    if (!start_x || !end_x || image_width < 0 || viewport_width <= 0 || margin < 0) {
        return false;
    }

    int available = viewport_width - 2 * margin;
    if (image_width <= available) {
        *start_x = (viewport_width - image_width) / 2;
        *end_x = *start_x;
        return false;
    }

    *start_x = margin;
    *end_x = viewport_width - margin - image_width;
    return true;
}

void eva_track_marquee_positions(int image_width, int gap, int margin, int offset,
                                  int *first_x, int *second_x)
{
    if (!first_x || !second_x) return;
    *first_x = margin - offset;
    *second_x = *first_x + image_width + gap;
}
