#include "eva_player.h"
#include "eva_player_model.h"
#include "eva_adpcm.h"
#include "fam1_format.h"
#include "audio_catalog.h"
#include "eva_music_store.h"
#include "eva_wifi.h"
#include "eva_title.h"
#include "eva_clock.h"
#include "eva_track_layout.h"
#include "eva_logo_assets.h"
#include "eva_text_assets.h"
#include "eva_text_buttons.h"
#include "eva_font_matisse.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define COLOR_BG 0x000000
#define COLOR_ORANGE 0xFF8800
#define COLOR_GREEN 0x5B7337
#define COLOR_MAGENTA 0xE9004D
#define SAMPLE_RATE 8000
#define AUDIO_CHUNK 256
#define UI_TICK_MS 33
#define TRACK_PANEL_W 232
#define TRACK_PANEL_H 46
#define TRACK_MARGIN 6
#define TRACK_MARQUEE_GAP 16
#define BATTERY_POLL_MS 30000U

static const char *TAG = "eva_player";

static lv_obj_t *s_scr;
static lv_obj_t *s_boot_scr;
static lv_obj_t *s_standby_scr;
static lv_obj_t *s_clock;
static lv_obj_t *s_battery_image;
static lv_obj_t *s_track_image;
static lv_obj_t *s_track_image_follow;
static lv_obj_t *s_track_fallback;
static lv_obj_t *s_upload_scr;
static lv_obj_t *s_upload_status;
static lv_obj_t *s_volume_scr;
static lv_obj_t *s_volume_value;
static lv_obj_t *s_volume_fill;
static lv_obj_t *s_buttons[4];
static lv_timer_t *s_ui_timer;
static lv_timer_t *s_boot_timer;
static uint32_t s_last_tick;
static uint32_t s_last_drawn_elapsed = UINT32_MAX;
static size_t s_displayed_track = SIZE_MAX;
static int s_track_image_width;
static eva_player_model_t s_model;
static bool s_ready;
static TaskHandle_t s_audio_task;
static volatile bool s_audio_playing;
static volatile bool s_audio_suspend;
static volatile bool s_audio_suspended;
static volatile bool s_audio_reset;
static volatile size_t s_audio_track;
static volatile bool s_audio_failed;
static volatile bool s_audio_track_finished;
static volatile size_t s_audio_finished_track;
static volatile uint32_t s_audio_finished_duration_ms;
static volatile bool s_boot_sound_done;
static uint32_t s_boot_started;
static bool s_storage_ready;
static bool s_storage_scan_done;
static bool s_battery_ready;
static int s_battery_bars = -1;
static uint32_t s_battery_next_poll;
static bool s_upload_mode;
static volatile uint8_t s_volume_percent = 70;

LV_DRAW_BUF_DEFINE_STATIC(s_clock_draw_buf, 224, 82, LV_COLOR_FORMAT_A8);

extern const uint8_t s_startup_adpcm_start[] asm("_binary_startup_adpcm_start");
extern const uint8_t s_startup_adpcm_end[] asm("_binary_startup_adpcm_end");

static void audio_task(void *arg);
static void boot_audio_task(void *arg);
static void sync_standby_screen(void);

typedef struct {
    uint32_t track;
    uint32_t generation;
} fam_reader_ctx_t;

static bool fam_reader_read(void *opaque, uint32_t offset, void *out, size_t len)
{
    fam_reader_ctx_t *ctx = opaque;
    return audio_catalog_read_at(ctx->track, ctx->generation, offset, out, len);
}

static lv_obj_t *panel(lv_obj_t *parent, int x, int y, int w, int h,
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

static void track_scroll_cb(void *obj, int32_t offset)
{
    int first_x;
    int second_x;
    eva_track_marquee_positions(s_track_image_width, TRACK_MARQUEE_GAP,
                                 TRACK_MARGIN, offset, &first_x, &second_x);
    lv_obj_set_x((lv_obj_t *)obj, first_x);
    lv_obj_set_x(s_track_image_follow, second_x);
}

static void update_track_image(void)
{
    if (!s_track_image) return;
    size_t track = eva_player_track_index(&s_model);
    if (track == s_displayed_track) return;

    lv_anim_delete(s_track_image, track_scroll_cb);
    const lv_image_dsc_t *image = NULL;
    if (track >= eva_player_track_count(&s_model) ||
        !eva_title_load((uint32_t)track, &image)) {
        char fallback[32];
        if (eva_player_track_count(&s_model) == 0)
            snprintf(fallback, sizeof(fallback), "%s", s_storage_ready ? "NO TRACKS" : "STORAGE ERROR");
        else
            snprintf(fallback, sizeof(fallback), "TRACK %02u", (unsigned)track + 1U);
        lv_label_set_text(s_track_fallback, fallback);
        lv_obj_center(s_track_fallback);
        lv_obj_remove_flag(s_track_fallback, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_track_image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_track_image_follow, LV_OBJ_FLAG_HIDDEN);
        s_displayed_track = track;
        return;
    }
    lv_obj_add_flag(s_track_fallback, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_track_image, LV_OBJ_FLAG_HIDDEN);
    lv_image_cache_drop(image);
    lv_image_set_src(s_track_image, NULL);
    lv_image_set_src(s_track_image_follow, NULL);
    lv_image_set_src(s_track_image, image);
    lv_image_set_src(s_track_image_follow, image);
    lv_obj_set_y(s_track_image, (TRACK_PANEL_H - (int)image->header.h) / 2);
    lv_obj_set_y(s_track_image_follow, (TRACK_PANEL_H - (int)image->header.h) / 2);

    int start_x;
    int end_x;
    bool scroll = eva_track_scroll_bounds((int)image->header.w, TRACK_PANEL_W,
                                          TRACK_MARGIN, &start_x, &end_x);
    if (scroll) {
        s_track_image_width = (int)image->header.w;
        lv_obj_remove_flag(s_track_image_follow, LV_OBJ_FLAG_HIDDEN);
        track_scroll_cb(s_track_image, 0);
        uint32_t cycle = (uint32_t)s_track_image_width + TRACK_MARQUEE_GAP;
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, s_track_image);
        lv_anim_set_exec_cb(&anim, track_scroll_cb);
        lv_anim_set_values(&anim, 0, (int32_t)cycle);
        lv_anim_set_duration(&anim, cycle * 40U);
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&anim);
    } else {
        lv_obj_set_x(s_track_image, start_x);
        lv_obj_add_flag(s_track_image_follow, LV_OBJ_FLAG_HIDDEN);
    }
    s_displayed_track = track;
}

static void draw_time(void)
{
    if (!s_clock) return;
    uint32_t elapsed = eva_player_elapsed_ms(&s_model);
    if (elapsed == s_last_drawn_elapsed) return;
    if (eva_clock_render_a8(s_clock_draw_buf.data, s_clock_draw_buf.header.stride, elapsed)) {
        s_last_drawn_elapsed = elapsed;
        lv_obj_invalidate(s_clock);
    }
}

static void update_button_styles(void)
{
    eva_player_control_t active = eva_player_active_control(&s_model);
    for (int i = 0; i < 4; i++) {
        uint32_t bg = active == (eva_player_control_t)i ? COLOR_MAGENTA : COLOR_BG;
        lv_obj_set_style_bg_color(s_buttons[i], lv_color_hex(bg), 0);
    }
}

static int battery_bar_count(int percent)
{
    if (percent <= 0) return 0;
    if (percent <= 33) return 1;
    if (percent <= 66) return 2;
    return 3;
}

static void update_battery_indicator(uint32_t now)
{
    if (!s_battery_image || (int32_t)(now - s_battery_next_poll) < 0) return;
    s_battery_next_poll = now + BATTERY_POLL_MS;
    int soc = s_battery_ready ? bsp_battery_soc() : -1;
    int bars = soc < 0 ? 0 : battery_bar_count(soc);
    if (bars == s_battery_bars) return;
    static const lv_image_dsc_t *const images[] = {
        &eva_text_internal_battery_0,
        &eva_text_internal_battery_1,
        &eva_text_internal_battery_2,
        &eva_text_internal_jp,
    };
    lv_image_set_src(s_battery_image, images[bars]);
    s_battery_bars = bars;
    ESP_LOGI(TAG, "battery: %d%%, bars=%d", soc, bars);
}

static bool sync_audio_snapshot(void)
{
    bool state_changed = false;
    if (s_audio_failed) {
        s_audio_failed = false;
        s_audio_task = NULL;
        eva_player_stop(&s_model);
        state_changed = true;
    }
    s_audio_playing = eva_player_is_playing(&s_model);
    s_audio_track = eva_player_track_index(&s_model);
    if (s_audio_playing && !s_audio_task) {
        BaseType_t ok = xTaskCreate(audio_task, "eva_audio", 4096, NULL, 4, &s_audio_task);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "audio task create failed");
            s_audio_task = NULL;
            eva_player_stop(&s_model);
            s_audio_playing = false;
            state_changed = true;
        }
    }
    return state_changed;
}

static bool consume_audio_track_finished(uint32_t now)
{
    if (!s_audio_track_finished) return false;

    size_t track = s_audio_finished_track;
    uint32_t duration_ms = s_audio_finished_duration_ms;
    s_audio_track_finished = false;
    if (track != eva_player_track_index(&s_model)) return false;

    ESP_LOGI(TAG, "track finished: track=%u duration=%u ms",
             (unsigned)track, (unsigned)duration_ms);
    eva_player_finish_track(&s_model, duration_ms, now);
    return true;
}

static void update_ui(void)
{
    sync_audio_snapshot();
    update_battery_indicator(lv_tick_get());
    draw_time();
    update_track_image();
    update_button_styles();
}

static void ui_tick(lv_timer_t *timer)
{
    (void)timer;
    uint32_t now = lv_tick_get();
    uint32_t delta = now - s_last_tick;
    s_last_tick = now;
    bool state_changed = consume_audio_track_finished(now);
    if (eva_player_auto_advance_due(&s_model, now)) {
        eva_player_advance_after_finish(&s_model);
        s_audio_reset = true;
        ESP_LOGI(TAG, "auto advance: track=%u",
                 (unsigned)eva_player_track_index(&s_model));
        delta = 0;
        state_changed = true;
    }
    state_changed = sync_audio_snapshot() || state_changed;
    update_battery_indicator(now);
    sync_standby_screen();
    if (eva_player_is_standby(&s_model)) return;

    if (eva_player_is_playing(&s_model)) {
        eva_player_set_elapsed_ms(&s_model, eva_player_elapsed_ms(&s_model) + delta);
    }
    if (state_changed) {
        update_track_image();
        update_button_styles();
    }
    draw_time();
}

static void make_button(lv_obj_t *parent, int index, const lv_image_dsc_t *icon, int x)
{
    s_buttons[index] = panel(parent, x, 266, 52, 30, COLOR_BG, COLOR_ORANGE);
    lv_obj_t *img = lv_image_create(s_buttons[index]);
    lv_image_set_src(img, icon);
    lv_obj_set_style_image_recolor(img, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_center(img);
}

static lv_obj_t *create_nerv_logo_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    lv_obj_t *logo = lv_image_create(scr);
    lv_image_set_src(logo, &eva_logo_nerv);
    lv_obj_center(logo);
    return scr;
}

static void build_player_screen(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);

    lv_obj_t *title_panel = panel(s_scr, 4, 14, 132, 34, COLOR_GREEN, COLOR_ORANGE);
    lv_obj_t *title_img = lv_image_create(title_panel);
    lv_image_set_src(title_img, &eva_text_evangelion);
    lv_obj_set_style_image_recolor(title_img, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_image_recolor_opa(title_img, LV_OPA_COVER, 0);
    lv_obj_center(title_img);
    lv_obj_t *internal_panel = panel(s_scr, 142, 12, 94, 44, COLOR_BG, COLOR_ORANGE);
    s_battery_image = lv_image_create(internal_panel);
    lv_image_set_src(s_battery_image, &eva_text_internal_battery_0);
    lv_obj_center(s_battery_image);

    lv_obj_t *system_panel = panel(s_scr, 142, 63, 94, 30, COLOR_BG, COLOR_ORANGE);
    lv_obj_t *system_img = lv_image_create(system_panel);
    lv_image_set_src(system_img, &eva_text_system_jp);
    lv_obj_center(system_img);

    lv_obj_t *status = lv_image_create(s_scr);
    lv_image_set_src(status, &eva_text_status);
    lv_obj_set_style_image_recolor(status, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_image_recolor_opa(status, LV_OPA_COVER, 0);
    lv_obj_set_pos(status, 9, 57);

    LV_DRAW_BUF_INIT_STATIC(s_clock_draw_buf);
    s_clock = lv_canvas_create(s_scr);
    lv_canvas_set_draw_buf(s_clock, &s_clock_draw_buf);
    lv_obj_set_pos(s_clock, 8, 106);
    lv_obj_set_style_image_recolor(s_clock, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_image_recolor_opa(s_clock, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_clock, 0, 0);
    lv_obj_set_style_border_color(s_clock, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_border_width(s_clock, 1, 0);

    lv_obj_t *track_panel = panel(s_scr, 4, 206, TRACK_PANEL_W, TRACK_PANEL_H, COLOR_BG, COLOR_ORANGE);
    s_track_image = lv_image_create(track_panel);
    lv_obj_set_style_image_recolor(s_track_image, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_image_recolor_opa(s_track_image, LV_OPA_COVER, 0);
    s_track_image_follow = lv_image_create(track_panel);
    lv_obj_set_style_image_recolor(s_track_image_follow, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_image_recolor_opa(s_track_image_follow, LV_OPA_COVER, 0);
    s_track_fallback = lv_label_create(track_panel);
    lv_obj_set_style_text_color(s_track_fallback, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_text_font(s_track_fallback, &eva_font_matisse_20, 0);
    lv_obj_center(s_track_fallback);

    panel(s_scr, 3, 258, 234, 45, COLOR_GREEN, COLOR_ORANGE);
    make_button(s_scr, EVA_PLAYER_CONTROL_PREV, &eva_text_btn_prev, 5);
    make_button(s_scr, EVA_PLAYER_CONTROL_PLAY, &eva_text_btn_play, 64);
    make_button(s_scr, EVA_PLAYER_CONTROL_NEXT, &eva_text_btn_next, 123);
    make_button(s_scr, EVA_PLAYER_CONTROL_PAUSE, &eva_text_btn_pause, 182);
    panel(s_scr, 3, 313, 234, 2, COLOR_GREEN, COLOR_GREEN);

    eva_player_init(&s_model, s_storage_ready ? audio_catalog_count() : 0);
    s_displayed_track = SIZE_MAX;
    s_battery_bars = -1;
    s_battery_next_poll = 0;
    s_last_tick = lv_tick_get();
    s_ready = true;
    update_ui();
    s_ui_timer = lv_timer_create(ui_tick, UI_TICK_MS, NULL);
    lv_screen_load(s_scr);
}

static void show_standby_screen(void)
{
    if (s_standby_scr) return;

    s_standby_scr = create_nerv_logo_screen();
    lv_screen_load(s_standby_scr);
}

static void hide_standby_screen(void)
{
    if (!s_standby_scr) return;

    lv_obj_t *standby = s_standby_scr;
    s_standby_scr = NULL;
    lv_screen_load(s_scr);
    lv_obj_delete(standby);
    update_ui();
}

static lv_obj_t *upload_label(lv_obj_t *parent, const char *text, int y,
                              const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_width(label, 224);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(label, 10, y);
    return label;
}

static void mode_header(lv_obj_t *screen, const lv_image_dsc_t *badge,
                        const lv_image_dsc_t *status, const char *system_text)
{
    lv_obj_t *title_panel = panel(screen, 4, 14, 132, 34, COLOR_GREEN, COLOR_ORANGE);
    lv_obj_t *title = lv_image_create(title_panel);
    lv_image_set_src(title, &eva_text_evangelion);
    lv_obj_set_style_image_recolor(title, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_image_recolor_opa(title, LV_OPA_COVER, 0);
    lv_obj_center(title);

    lv_obj_t *badge_panel = panel(screen, 142, 12, 94, 44, COLOR_BG, COLOR_ORANGE);
    lv_obj_t *badge_image = lv_image_create(badge_panel);
    lv_image_set_src(badge_image, badge);
    lv_obj_center(badge_image);

    lv_obj_t *system_panel = panel(screen, 142, 63, 94, 30, COLOR_BG, COLOR_ORANGE);
    lv_obj_t *system = lv_label_create(system_panel);
    lv_label_set_text(system, system_text);
    lv_obj_set_style_text_color(system, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_text_font(system, &eva_font_matisse_14, 0);
    lv_obj_center(system);

    lv_obj_t *status_image = lv_image_create(screen);
    lv_image_set_src(status_image, status);
    lv_obj_set_style_image_recolor(status_image, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_image_recolor_opa(status_image, LV_OPA_COVER, 0);
    lv_obj_set_pos(status_image, 9, 57);
    panel(screen, 3, 313, 234, 2, COLOR_GREEN, COLOR_GREEN);
}

static void mode_instruction(lv_obj_t *screen, const lv_image_dsc_t *instruction,
                             const char *exit_text)
{
    panel(screen, 3, 258, 234, 45, COLOR_GREEN, COLOR_ORANGE);
    if (instruction) {
        lv_obj_t *image = lv_image_create(screen);
        lv_image_set_src(image, instruction);
        lv_obj_set_style_image_recolor(image, lv_color_hex(COLOR_ORANGE), 0);
        lv_obj_set_style_image_recolor_opa(image, LV_OPA_COVER, 0);
        lv_obj_set_pos(image, 10, 260);
    }
    lv_obj_t *exit_label = upload_label(screen, exit_text, instruction ? 286 : 271,
                                       &eva_font_matisse_14, COLOR_ORANGE);
    lv_obj_set_style_text_align(exit_label, LV_TEXT_ALIGN_CENTER, 0);
}

static void draw_volume(void)
{
    if (!s_volume_value) return;
    lv_label_set_text_fmt(s_volume_value, "%u%%", (unsigned)s_volume_percent);
    lv_obj_align(s_volume_value, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_width(s_volume_fill, 2U * s_volume_percent);
}

static void enter_volume_mode(void)
{
    s_volume_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_volume_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_volume_scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(s_volume_scr, 0, 0);
    lv_obj_set_style_pad_all(s_volume_scr, 0, 0);
    mode_header(s_volume_scr, &eva_text_volume_badge, &eva_text_volume_status,
                "AUDIO");

    lv_obj_t *level_panel = panel(s_volume_scr, 8, 106, 224, 82, COLOR_BG, COLOR_ORANGE);
    lv_obj_t *heading = upload_label(level_panel, "MASTER VOLUME", 8,
                                     &eva_font_matisse_14, COLOR_ORANGE);
    lv_obj_set_width(heading, 210);
    s_volume_value = lv_label_create(level_panel);
    lv_obj_set_style_text_color(s_volume_value, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_text_font(s_volume_value, &eva_font_matisse_20, 0);

    lv_obj_t *bar = panel(s_volume_scr, 4, 206, 232, 46, COLOR_BG, COLOR_ORANGE);
    s_volume_fill = panel(bar, 12, 15, 0, 14, COLOR_ORANGE, COLOR_ORANGE);
    mode_instruction(s_volume_scr, &eva_text_volume_instruction, "OK / RETURN");
    draw_volume();
    lv_screen_load(s_volume_scr);
}

static void exit_volume_mode(void)
{
    if (s_storage_ready) {
        esp_err_t err = eva_music_store_save_volume(s_volume_percent);
        if (err != ESP_OK) ESP_LOGE(TAG, "volume save failed: %s", esp_err_to_name(err));
    }
    lv_obj_t *screen = s_volume_scr;
    s_volume_scr = NULL;
    s_volume_value = NULL;
    s_volume_fill = NULL;
    lv_screen_load(s_scr);
    lv_obj_delete(screen);
    s_model.active_control = eva_player_is_playing(&s_model) ?
                             EVA_PLAYER_CONTROL_PLAY : EVA_PLAYER_CONTROL_PAUSE;
    update_ui();
}

static void enter_upload_mode(void)
{
    if (!s_storage_ready) return;
    s_audio_suspend = true;
    s_audio_playing = false;
    for (int i = 0; s_audio_task && i < 100; i++)
        vTaskDelay(pdMS_TO_TICKS(10));
    if (s_audio_task) {
        ESP_LOGE(TAG, "audio did not release hardware for upload");
        s_audio_suspend = false;
        return;
    }
    s_upload_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_upload_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_upload_scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(s_upload_scr, 0, 0);
    lv_obj_set_style_pad_all(s_upload_scr, 0, 0);
    mode_header(s_upload_scr, &eva_text_wifi_badge, &eva_text_wifi_status,
                "TRANSFER");
    lv_obj_t *network_panel = panel(s_upload_scr, 8, 106, 224, 82,
                                    COLOR_BG, COLOR_ORANGE);
    lv_obj_t *address_panel = panel(s_upload_scr, 4, 206, 232, 46,
                                    COLOR_BG, COLOR_ORANGE);
    esp_err_t err = eva_wifi_start();
    if (err == ESP_OK) {
        s_upload_status = upload_label(network_panel, eva_wifi_ssid(), 27,
                                       &eva_font_matisse_20, COLOR_MAGENTA);
        lv_obj_set_width(s_upload_status, 210);
        lv_obj_t *address = upload_label(address_panel, "http://192.168.4.1/", 12,
                                         &eva_font_matisse_14, COLOR_ORANGE);
        lv_obj_set_width(address, 210);
        lv_obj_set_style_text_align(address, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        ESP_LOGE(TAG, "Wi-Fi start failed: %s", esp_err_to_name(err));
        s_upload_status = upload_label(network_panel, "WIFI START FAILED", 27,
                                       &eva_font_matisse_14, COLOR_MAGENTA);
    }
    mode_instruction(s_upload_scr, NULL, "HOLD DOWN / EXIT");
    s_upload_mode = true;
    lv_screen_load(s_upload_scr);
}

static void exit_upload_mode(void)
{
    if (eva_wifi_stop() != ESP_OK) {
        lv_label_set_text(s_upload_status, "UPLOAD IN PROGRESS");
        return;
    }
    s_upload_mode = false;
    s_audio_reset = true;
    s_audio_suspend = false;
    eva_player_set_track_count(&s_model, audio_catalog_count());
    eva_player_set_elapsed_ms(&s_model, 0);
    s_displayed_track = SIZE_MAX;
    s_last_tick = lv_tick_get();
    lv_obj_t *screen = s_upload_scr;
    s_upload_scr = NULL;
    s_upload_status = NULL;
    lv_screen_load(s_scr);
    lv_obj_delete(screen);
    update_ui();
}

static void sync_standby_screen(void)
{
    if (eva_player_is_standby(&s_model)) {
        show_standby_screen();
    } else {
        hide_standby_screen();
    }
}

static void boot_done(lv_timer_t *timer)
{
    if (!s_boot_sound_done || !s_storage_scan_done ||
        lv_tick_get() - s_boot_started < 3000U) return;
    lv_timer_delete(timer);
    s_boot_timer = NULL;
    lv_obj_t *boot = s_boot_scr;
    build_player_screen();
    if (boot) {
        lv_obj_delete(boot);
        s_boot_scr = NULL;
    }
}

static void build_boot_screen(void)
{
    lv_obj_t *boot = create_nerv_logo_screen();
    s_boot_scr = boot;
    lv_screen_load(boot);
    lv_refr_now(lv_display_get_default());
    vTaskDelay(pdMS_TO_TICKS(40));
    bsp_display_backlight(100);
    s_boot_started = lv_tick_get();
    s_boot_sound_done = false;
    if (xTaskCreate(boot_audio_task, "eva_intro", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "startup audio task create failed");
        s_boot_sound_done = true;
    }
    s_boot_timer = lv_timer_create(boot_done, 50, NULL);
}

static void audio_task(void *arg)
{
    (void)arg;
    size_t current_track = SIZE_MAX;
    uint32_t generation = 0;
    uint32_t offset = 0;
    bool hardware_ready = false;
    int applied_volume = -1;
    fam1_header_t header = { 0 };
    s_audio_suspended = false;
    for (;;) {
        if (s_audio_suspend) {
            if (hardware_ready) bsp_audio_deinit();
            s_audio_suspended = true;
            s_audio_task = NULL;
            vTaskDelete(NULL);
        }
        if (!s_audio_playing) {
            if (hardware_ready) {
                bsp_audio_deinit();
                hardware_ready = false;
                applied_volume = -1;
            }
            s_audio_suspended = true;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        s_audio_suspended = false;
        size_t track = s_audio_track;
        if (s_audio_reset || track != current_track) {
            s_audio_reset = false;
            current_track = track;
            audio_track_t info;
            if (!audio_catalog_get((uint32_t)track, &info)) {
                ESP_LOGE(TAG, "track missing: %u", (unsigned)track);
                goto audio_failed;
            }
            generation = audio_catalog_generation();
            fam_reader_ctx_t ctx = { (uint32_t)track, generation };
            fam1_reader_t reader = { .read = fam_reader_read, .ctx = &ctx };
            if (!fam1_read_header(&reader, info.size, &header)) {
                ESP_LOGE(TAG, "track header invalid: %u", (unsigned)track);
                goto audio_failed;
            }
            offset = header.first_block_offset;
            if (hardware_ready) {
                bsp_audio_deinit();
                hardware_ready = false;
                applied_volume = -1;
            }
        }
        if (!hardware_ready) {
            if (bsp_audio_init() != ESP_OK ||
                bsp_audio_set_format(header.sample_rate, 16, 1) != ESP_OK) {
                ESP_LOGE(TAG, "audio hardware init failed");
                goto audio_failed;
            }
            hardware_ready = true;
        }
        if (applied_volume != s_volume_percent) {
            bsp_audio_set_volume(s_volume_percent);
            applied_volume = s_volume_percent;
        }
        uint8_t block[8U + FAM1_BLOCK_SAMPLES / 2U];
        if (!audio_catalog_read_at((uint32_t)track, generation, offset, block, 8U)) {
            ESP_LOGE(TAG, "audio block header read failed");
            goto audio_failed;
        }
        uint16_t encoded = (uint16_t)block[6] | ((uint16_t)block[7] << 8);
        size_t block_size = 8U + encoded;
        if (block_size > sizeof(block) ||
            !audio_catalog_read_at((uint32_t)track, generation, offset, block, block_size)) {
            ESP_LOGE(TAG, "audio block read failed");
            goto audio_failed;
        }
        int16_t pcm[FAM1_BLOCK_SAMPLES];
        fam1_block_t decoded;
        if (!fam1_decode_block(block, block_size, pcm, FAM1_BLOCK_SAMPLES, &decoded) ||
            bsp_audio_write(pcm, (size_t)decoded.sample_count * sizeof(pcm[0])) != ESP_OK) {
            ESP_LOGE(TAG, "audio decode or output failed");
            goto audio_failed;
        }
        offset += decoded.next_offset;
        if (offset >= header.file_size) {
            s_audio_finished_track = current_track;
            s_audio_finished_duration_ms = header.duration_ms;
            s_audio_track_finished = true;
            s_audio_playing = false;
        }
    }
audio_failed:
    bsp_audio_deinit();
    s_audio_suspended = true;
    s_audio_failed = true;
    vTaskDelete(NULL);
}

static void boot_audio_task(void *arg)
{
    (void)arg;
    eva_adpcm_decoder_t decoder = { 0 };
    if (!eva_adpcm_reset(&decoder, s_startup_adpcm_start, s_startup_adpcm_end,
                         SAMPLE_RATE) || bsp_audio_init() != ESP_OK ||
        bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "startup sound unavailable");
        goto done;
    }
    bsp_audio_set_volume(s_volume_percent);
    int16_t pcm[AUDIO_CHUNK];
    while (!eva_adpcm_finished(&decoder)) {
        size_t count = 0;
        while (count < AUDIO_CHUNK && !eva_adpcm_finished(&decoder))
            pcm[count++] = eva_adpcm_next(&decoder);
        if (bsp_audio_write(pcm, count * sizeof(pcm[0])) != ESP_OK) {
            ESP_LOGE(TAG, "startup sound output failed");
            break;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
done:
    bsp_audio_deinit();
    s_boot_sound_done = true;
    vTaskDelete(NULL);
}

void eva_player_start(bool storage_ready, bool battery_ready)
{
    s_storage_ready = storage_ready;
    s_storage_scan_done = storage_ready;
    s_battery_ready = battery_ready;
    s_battery_image = NULL;
    s_volume_percent = 70;
    uint8_t saved_volume;
    esp_err_t err = eva_music_store_load_volume(&saved_volume);
    if (err == ESP_OK && saved_volume <= 100) s_volume_percent = saved_volume;
    else if (err == ESP_OK) ESP_LOGW(TAG, "saved volume out of range: %u", saved_volume);
    else if (err != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "volume load failed: %s", esp_err_to_name(err));
    s_audio_failed = false;
    s_audio_track_finished = false;
    s_audio_suspend = false;
    s_audio_suspended = true;
    s_audio_reset = false;
    s_audio_task = NULL;
    s_upload_mode = false;
    s_volume_scr = NULL;
    s_volume_value = NULL;
    s_volume_fill = NULL;
    s_standby_scr = NULL;
    s_ready = false;
    build_boot_screen();

}

void eva_player_set_storage_ready(bool storage_ready)
{
    s_storage_ready = storage_ready;
    s_storage_scan_done = true;
    if (!s_ready) return;
    eva_player_set_track_count(&s_model, storage_ready ? audio_catalog_count() : 0);
    s_displayed_track = SIZE_MAX;
    update_ui();
}

void eva_player_handle_button(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!s_ready) return;

    if (s_volume_scr) {
        if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            exit_volume_mode();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            s_volume_percent = s_volume_percent <= 95 ? s_volume_percent + 5 : 100;
            draw_volume();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            s_volume_percent = s_volume_percent >= 5 ? s_volume_percent - 5 : 0;
            draw_volume();
        }
        return;
    }
    if (s_upload_mode) {
        if (btn == BSP_BTN_DOWN && ev == BSP_BTN_LONG) exit_upload_mode();
        return;
    }
    if (btn == BSP_BTN_UP && ev == BSP_BTN_LONG &&
        !eva_player_is_standby(&s_model)) {
        enter_volume_mode();
        return;
    }
    if (btn == BSP_BTN_DOWN && ev == BSP_BTN_LONG &&
        !eva_player_is_playing(&s_model) &&
        !eva_player_is_standby(&s_model) &&
        !eva_player_auto_advance_pending(&s_model)) {
        enter_upload_mode();
        return;
    }

    eva_player_key_t key;
    if (btn == BSP_BTN_UP) key = EVA_PLAYER_KEY_UP;
    else if (btn == BSP_BTN_DOWN) key = EVA_PLAYER_KEY_DOWN;
    else if (btn == BSP_BTN_OK) key = EVA_PLAYER_KEY_OK;
    else return;

    eva_player_key_event_t event;
    if (ev == BSP_BTN_PRESS) event = EVA_PLAYER_KEY_PRESS;
    else if (ev == BSP_BTN_CLICK) event = EVA_PLAYER_KEY_CLICK;
    else if (ev == BSP_BTN_LONG) event = EVA_PLAYER_KEY_LONG;
    else return;

    bool was_auto_advance_pending = eva_player_auto_advance_pending(&s_model);
    bool was_standby = eva_player_is_standby(&s_model);
    eva_player_handle_key(&s_model, key, event);
    if (event == EVA_PLAYER_KEY_CLICK &&
        (key == EVA_PLAYER_KEY_UP || key == EVA_PLAYER_KEY_DOWN ||
         (key == EVA_PLAYER_KEY_OK && was_auto_advance_pending)))
        s_audio_reset = true;
    bool is_standby = eva_player_is_standby(&s_model);
    if (was_auto_advance_pending ||
        was_standby != is_standby ||
        (event == EVA_PLAYER_KEY_CLICK &&
         (key == EVA_PLAYER_KEY_UP || key == EVA_PLAYER_KEY_DOWN))) {
        s_last_tick = lv_tick_get();
    }
    if (was_standby != is_standby) {
        sync_standby_screen();
    } else if (!is_standby) {
        update_ui();
    }
}
