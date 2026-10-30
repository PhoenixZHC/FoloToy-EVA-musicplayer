#include "eva_player_frame.h"
#include "eva_text_assets.h"

lv_obj_t *eva_player_frame_panel(lv_obj_t *parent, int x, int y, int w, int h,
    uint32_t bg, uint32_t border)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    return obj;
}

lv_obj_t *eva_player_frame_header(lv_obj_t *parent, const char *mode, const lv_font_t *font)
{
    lv_obj_t *title = eva_player_frame_panel(parent, 4, 14, 132, 34, EVA_FRAME_GREEN, EVA_FRAME_ORANGE);
    lv_obj_t *image = lv_image_create(title);
    lv_image_set_src(image, &eva_text_evangelion);
    lv_obj_set_style_image_recolor(image, lv_color_hex(EVA_FRAME_ORANGE), 0);
    lv_obj_set_style_image_recolor_opa(image, LV_OPA_COVER, 0);
    lv_obj_center(image);
    lv_obj_t *battery = eva_player_frame_panel(parent, 142, 12, 94, 44, EVA_FRAME_BG, EVA_FRAME_ORANGE);
    lv_obj_t *battery_image = lv_image_create(battery);
    lv_image_set_src(battery_image, &eva_text_internal_battery_0);
    lv_obj_center(battery_image);
    lv_obj_t *system = eva_player_frame_panel(parent, 142, 63, 94, 30, EVA_FRAME_BG, EVA_FRAME_ORANGE);
    if (mode) {
        lv_obj_t *label = lv_label_create(system);
        lv_label_set_text(label, mode);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(EVA_FRAME_ORANGE), 0);
        lv_obj_center(label);
    } else {
        image = lv_image_create(system);
        lv_image_set_src(image, &eva_text_system_jp);
        lv_obj_center(image);
    }
    if (!mode) {
        image = lv_image_create(parent);
        lv_image_set_src(image, &eva_text_status);
        lv_obj_set_style_image_recolor(image, lv_color_hex(EVA_FRAME_ORANGE), 0);
        lv_obj_set_style_image_recolor_opa(image, LV_OPA_COVER, 0);
        lv_obj_set_pos(image, 9, 57);
    }
    return battery_image;
}

void eva_player_frame_battery(lv_obj_t *image, int bars)
{
    static const lv_image_dsc_t *const images[] = {
        &eva_text_internal_battery_0, &eva_text_internal_battery_1,
        &eva_text_internal_battery_2, &eva_text_internal_jp,
    };
    if (bars < 0 || bars > 3) bars = 0;
    lv_image_set_src(image, images[bars]);
}
