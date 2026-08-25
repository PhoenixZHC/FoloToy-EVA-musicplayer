#include <assert.h>
#include "eva_track_layout.h"

static void test_overflowing_title_uses_a_forward_marquee_cycle(void)
{
    int start = 0;
    int end = 0;
    int first = 0;
    int second = 0;

    assert(eva_track_scroll_bounds(260, 232, 6, &start, &end));
    assert(start == 6);
    assert(end == -34);

    eva_track_marquee_positions(260, 16, 6, 0, &first, &second);
    assert(first == 6);
    assert(second == 282);

    eva_track_marquee_positions(260, 16, 6, 276, &first, &second);
    assert(first == -270);
    assert(second == 6);
}

static void test_short_title_is_centered_without_scrolling(void)
{
    int start = 0;
    int end = 0;

    assert(!eva_track_scroll_bounds(190, 232, 6, &start, &end));
    assert(start == 21);
    assert(end == 21);
}

int main(void)
{
    test_overflowing_title_uses_a_forward_marquee_cycle();
    test_short_title_is_centered_without_scrolling();
    return 0;
}
