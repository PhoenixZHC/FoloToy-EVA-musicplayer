#pragma once

#include "lvgl.h"

#define EVA_FRAME_BG 0x000000
#define EVA_FRAME_ORANGE 0xFF8800
#define EVA_FRAME_GREEN 0x5B7337
#define EVA_FRAME_MAGENTA 0xE9004D

lv_obj_t *eva_player_frame_panel(lv_obj_t *parent, int x, int y, int w, int h,
    uint32_t bg, uint32_t border);
lv_obj_t *eva_player_frame_header(lv_obj_t *parent, const char *mode, const lv_font_t *font);
void eva_player_frame_battery(lv_obj_t *image, int bars);
