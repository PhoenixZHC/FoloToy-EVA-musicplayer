#pragma once
#include "lvgl.h"
#include <stddef.h>

LV_FONT_DECLARE(eva_font_radio_16);
LV_FONT_DECLARE(eva_font_radio_20);

typedef struct {
    lv_obj_t *screen, *city, *station, *detail, *status, *battery, *controls[4];
    int battery_bars, playing;
} eva_radio_view_t;
typedef struct { lv_obj_t *screen, *heading, *panels[5], *rows[5], *position; } eva_radio_list_view_t;

void eva_radio_view_create(eva_radio_view_t *view);
void eva_radio_view_playing(eva_radio_view_t *view, bool playing);
void eva_radio_view_selection(eva_radio_view_t *view, const char *city,
    const char *station, const char *genre, size_t index, size_t count);
void eva_radio_view_battery(eva_radio_view_t *view, int bars);
void eva_radio_list_create(eva_radio_list_view_t *view, int battery_bars);
void eva_radio_list_draw(eva_radio_list_view_t *view, const char *heading,
    const char *const *names, size_t count, size_t selected);
