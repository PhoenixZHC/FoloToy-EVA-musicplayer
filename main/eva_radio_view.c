#include "eva_radio_view.h"
#include "eva_player_frame.h"

#define BG 0x000000
#define ORANGE 0xFF8800
#define GREEN EVA_FRAME_GREEN

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int w,
    const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, w);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    return obj;
}

static lv_obj_t *screen(void)
{
    lv_obj_t *obj = lv_obj_create(NULL);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(BG), 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    return obj;
}

void eva_radio_view_create(eva_radio_view_t *view)
{
    *view = (eva_radio_view_t){0};
    view->battery_bars = view->playing = -1;
    view->screen = screen();
    view->battery = eva_player_frame_header(view->screen, "网络电台", &eva_font_radio_16);
    /* 原文字区显示城市/台序，原数码区显示台名，原歌名区显示直播状态。 */
    view->city = label(view->screen, "城市", 9, 55, 126, &eva_font_radio_20, ORANGE);
    view->detail = label(view->screen, "电台 01/01", 9, 80, 126, &eva_font_radio_16, ORANGE);
    lv_obj_t *station = eva_player_frame_panel(view->screen, 8, 106, 224, 82, BG, ORANGE);
    view->station = label(station, "正在加载电台", 0, 0, 210, &eva_font_radio_20, ORANGE);
    lv_obj_set_style_text_align(view->station, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(view->station);
    label(view->screen, "MP3 / 双击OK选择城市", 9, 188, 222, &eva_font_radio_16, ORANGE);
    lv_obj_t *state = eva_player_frame_panel(view->screen, 4, 210, 232, 42, BG, ORANGE);
    view->indicator = eva_player_frame_panel(state, 8, 16, 8, 8, ORANGE, ORANGE);
    view->status = label(state, "正在连接电台", 0, 0, 200, &eva_font_radio_16, ORANGE);
    lv_obj_set_style_text_align(view->status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view->status, LV_ALIGN_CENTER, 9, 0);
    eva_player_frame_panel(view->screen, 3, 258, 234, 45, GREEN, ORANGE);
    static const char *const controls[] = { "上台", "播放", "下台", "暂停" };
    for (int i = 0; i < 4; i++) {
        view->controls[i] = eva_player_frame_panel(view->screen, 5 + i * 59, 266, 52, 30, BG, ORANGE);
        lv_obj_t *text = label(view->controls[i], controls[i], 0, 0, 48, &eva_font_radio_16, ORANGE);
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(text);
    }
    eva_player_frame_panel(view->screen, 3, 313, 234, 2, GREEN, GREEN);
}

void eva_radio_view_selection(eva_radio_view_t *view, const char *city,
    const char *station, const char *genre, size_t index, size_t count)
{
    lv_label_set_text(view->city, city);
    lv_label_set_text_fmt(view->detail, "%s %02u/%02u", genre, (unsigned)(index + 1), (unsigned)count);
    lv_label_set_text(view->station, station);
}

void eva_radio_view_battery(eva_radio_view_t *view, int bars)
{
    if (bars < 0 || bars > 3) bars = 0;
    if (bars == view->battery_bars) return;
    eva_player_frame_battery(view->battery, bars);
    view->battery_bars = bars;
}

void eva_radio_view_playing(eva_radio_view_t *view, bool playing)
{
    if ((int)playing == view->playing) return;
    lv_obj_set_style_bg_color(view->indicator, lv_color_hex(playing ? EVA_FRAME_MAGENTA : ORANGE), 0);
    for (int i = 0; i < 4; i++) {
        bool active = playing ? i == 1 : i == 3;
        lv_obj_set_style_bg_color(view->controls[i], lv_color_hex(active ? EVA_FRAME_MAGENTA : BG), 0);
    }
    view->playing = playing;
}

void eva_radio_list_create(eva_radio_list_view_t *view, int battery_bars)
{
    *view = (eva_radio_list_view_t){0};
    view->screen = screen();
    lv_obj_t *battery = eva_player_frame_header(view->screen, "城市电台", &eva_font_radio_16);
    eva_player_frame_battery(battery, battery_bars);
    view->heading = label(view->screen, "选择城市", 9, 60, 126, &eva_font_radio_16, ORANGE);
    for (int i = 0; i < 5; i++) {
        view->panels[i] = eva_player_frame_panel(view->screen, 8, 120 + i * 32, 224, 30, BG, ORANGE);
        view->rows[i] = label(view->panels[i], "", 5, 4, 212, &eva_font_radio_16, ORANGE);
        lv_label_set_long_mode(view->rows[i], LV_LABEL_LONG_DOT);
    }
    lv_obj_t *position = eva_player_frame_panel(view->screen, 3, 286, 234, 28, GREEN, ORANGE);
    view->position = label(position, "", 5, 3, 222, &eva_font_radio_16, ORANGE);
    lv_obj_set_style_text_align(view->position, LV_TEXT_ALIGN_CENTER, 0);
}

void eva_radio_list_draw(eva_radio_list_view_t *view, const char *heading,
    const char *const *names, size_t count, size_t selected)
{
    lv_label_set_text(view->heading, heading);
    size_t start = selected > 2 ? selected - 2 : 0;
    if (count > 5 && start > count - 5) start = count - 5;
    for (size_t i = 0; i < 5; i++) {
        size_t index = start + i;
        bool active = index < count && index == selected;
        lv_label_set_text(view->rows[i], index < count ? names[index] : "");
        lv_obj_set_style_bg_color(view->panels[i], lv_color_hex(active ? GREEN : BG), 0);
        lv_obj_set_style_border_color(view->panels[i], lv_color_hex(active ? EVA_FRAME_MAGENTA : ORANGE), 0);
        lv_obj_set_style_text_color(view->rows[i], lv_color_hex(ORANGE), 0);
    }
    lv_label_set_text_fmt(view->position, "第 %u 项 / 共 %u 项", (unsigned)(selected + 1), (unsigned)count);
}
