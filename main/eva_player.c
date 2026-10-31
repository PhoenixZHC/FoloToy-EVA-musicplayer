#include "eva_player.h"
#include "eva_player_frame.h"
#include "eva_player_model.h"
#include "eva_settings.h"
#include "eva_settings_store.h"
#include "eva_adpcm.h"
#include "fam1_format.h"
#include "audio_catalog.h"
#include "eva_music_store.h"
#include "eva_wifi.h"
#include "eva_radio_player.h"
#include "eva_radio_catalog.h"
#include "eva_radio_config.h"
#include "eva_radio_view.h"
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
#include "esp_app_desc.h"
#include "nvs.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
static lv_obj_t *s_buttons[4];
static lv_timer_t *s_ui_timer;
static lv_timer_t *s_boot_timer;
static uint32_t s_last_tick;
static uint32_t s_last_drawn_elapsed = UINT32_MAX;
static size_t s_displayed_track = SIZE_MAX;
static int s_track_image_width;
static eva_player_model_t s_model;
static bool s_ready;
static TaskHandle_t volatile s_audio_task;
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
static uint8_t s_saved_volume;
static bool s_radio_mode;
static size_t s_radio_station;
static lv_obj_t *s_radio_scr, *s_radio_title, *s_radio_status;
static eva_radio_view_t s_radio_view;
static eva_radio_list_view_t s_radio_list;
static unsigned s_radio_list_stage;
static size_t s_radio_list_city, s_radio_list_selected;
static bool s_radio_exit, s_radio_start_pending, s_radio_local_released;
static size_t s_radio_city;
static uint16_t s_radio_saved_station;
static esp_err_t s_radio_start_error;
static eva_settings_t s_settings, s_saved_settings;
static lv_obj_t *s_settings_scr, *s_settings_rows[EVA_SETTING_COUNT];
static lv_obj_t *s_settings_labels[EVA_SETTING_COUNT], *s_settings_hint;
static eva_setting_item_t s_settings_item;
static bool s_settings_edit, s_settings_help, s_screen_off, s_wake_key;
static lv_obj_t *s_settings_help_panel;
static uint32_t s_last_activity;
typedef enum { SOURCE_REQUEST_NONE, SOURCE_REQUEST_LOCAL, SOURCE_REQUEST_RADIO,
               SOURCE_REQUEST_NETWORK } source_request_t;
static source_request_t s_source_request;

LV_DRAW_BUF_DEFINE_STATIC(s_clock_draw_buf, 224, 82, LV_COLOR_FORMAT_A8);

extern const uint8_t s_startup_adpcm_start[] asm("_binary_startup_adpcm_start");
extern const uint8_t s_startup_adpcm_end[] asm("_binary_startup_adpcm_end");

static void audio_task(void *arg);
static void boot_audio_task(void *arg);
static void sync_standby_screen(void);
static void radio_tick(void);
static void enter_radio_mode(void);
static void process_source_request(void);
static void screen_idle_tick(uint32_t now);

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
    return eva_player_frame_panel(parent, x, y, w, h, bg, border);
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
    if (s_radio_mode) eva_radio_view_battery(&s_radio_view, bars);
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
    if (s_radio_mode || s_audio_suspend) return false;
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
        TaskHandle_t task = NULL;
        vTaskSuspendAll();
        BaseType_t ok = xTaskCreate(audio_task, "eva_audio", 4096, NULL, 4, &task);
        s_audio_task = task;
        xTaskResumeAll();
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
    if (s_radio_mode) {
        eva_radio_player_volume(s_volume_percent);
        return;
    }
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
    static uint32_t last_memory_report;
    if ((uint32_t)(now - last_memory_report) >= 30000) {
        lv_mem_monitor_t memory;
        lv_mem_monitor(&memory);
        ESP_LOGI(TAG, "UI memory: used=%u peak=%u largest=%u",
            (unsigned)(memory.total_size - memory.free_size),
            (unsigned)memory.max_used, (unsigned)memory.free_biggest_size);
        last_memory_report = now;
    }
    s_last_tick = now;
    screen_idle_tick(now);
    if (!s_settings_scr) process_source_request();
    if (s_radio_mode) {
        radio_tick();
        update_battery_indicator(now);
        return;
    }
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

    s_battery_image = eva_player_frame_header(s_scr, NULL, NULL);

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
    lv_obj_t *hint = lv_label_create(s_standby_scr);
    lv_label_set_text(hint, "STANDBY\nCLICK OK BACK / HOLD OK SETTINGS");
    lv_obj_set_style_text_font(hint, &eva_font_matisse_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(hint, 224);
    lv_obj_set_pos(hint, 8, 270);
    ESP_LOGI(TAG, "standby entered: click OK back, hold OK settings");
    lv_screen_load(s_standby_scr);
}

static void hide_standby_screen(void)
{
    if (!s_standby_scr) return;
    ESP_LOGI(TAG, "standby exit");

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

static void radio_selection_draw(void)
{
    const eva_radio_station_t *station;
    const eva_radio_city_t *city;
    if (!eva_radio_city_at(s_radio_city, &city) ||
        !eva_radio_station_at(s_radio_station, &station)) return;
    size_t count = eva_radio_stations_for_city(city->id), index = 0;
    for (size_t i = 0; i < count; i++) {
        const eva_radio_station_t *candidate;
        eva_radio_station_for_city_at(city->id, i, &candidate);
        if (candidate->id == station->id) index = i;
    }
    eva_radio_view_selection(&s_radio_view, city->name, station->name, station->category, index, count);
}

static void radio_list_draw(void)
{
    const char *names[32];
    size_t count = eva_radio_city_count();
    const eva_radio_city_t *city;
    char heading[64] = "选择城市";
    if (s_radio_list_stage == 2) {
        eva_radio_city_at(s_radio_list_city, &city);
        count = eva_radio_stations_for_city(city->id);
        snprintf(heading, sizeof(heading), "%s电台", city->name);
    }
    if (!count || count > 32) { ESP_LOGE(TAG, "invalid radio list size: %u", (unsigned)count); return; }
    for (size_t i = 0; i < count; i++) {
        if (s_radio_list_stage == 1) {
            eva_radio_city_at(i, &city);
            names[i] = city->name;
        } else {
            const eva_radio_station_t *station;
            eva_radio_station_for_city_at(city->id, i, &station);
            names[i] = station->name;
        }
    }
    eva_radio_list_draw(&s_radio_list, heading, names, count, s_radio_list_selected);
}

static void close_radio_list(void)
{
    lv_obj_t *screen = s_radio_list.screen;
    s_radio_list.screen = NULL;
    s_radio_list_stage = 0;
    lv_screen_load(s_radio_scr);
    lv_obj_delete(screen);
}

static void open_radio_list(void)
{
    s_radio_list_stage = 1;
    s_radio_list_selected = s_radio_city;
    s_radio_list_city = s_radio_city;
    eva_radio_list_create(&s_radio_list, s_battery_bars);
    radio_list_draw();
    lv_screen_load(s_radio_list.screen);
}

static void radio_list_button(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (btn == BSP_BTN_DOWN && ev == BSP_BTN_LONG) {
        if (s_radio_list_stage == 2) {
            s_radio_list_stage = 1;
            s_radio_list_selected = s_radio_list_city;
            radio_list_draw();
        } else close_radio_list();
        return;
    }
    if (ev != BSP_BTN_CLICK) return;
    const eva_radio_city_t *city;
    eva_radio_city_at(s_radio_list_city, &city);
    size_t count = s_radio_list_stage == 1 ? eva_radio_city_count() : eva_radio_stations_for_city(city->id);
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        int direction = btn == BSP_BTN_UP ? -1 : 1;
        s_radio_list_selected = (s_radio_list_selected + count + direction) % count;
    } else if (btn == BSP_BTN_OK) {
        if (s_radio_list_stage == 1) {
            s_radio_list_city = s_radio_list_selected;
            s_radio_list_stage = 2;
            s_radio_list_selected = 0;
        } else {
            const eva_radio_station_t *station;
            eva_radio_station_for_city_at(city->id, s_radio_list_selected, &station);
            s_radio_city = s_radio_list_city;
            for (size_t i = 0; i < eva_radio_station_count(); i++) {
                const eva_radio_station_t *candidate;
                eva_radio_station_at(i, &candidate);
                if (candidate->id == station->id) s_radio_station = i;
            }
            s_radio_start_pending = true;
            eva_radio_player_stop();
            radio_selection_draw();
            close_radio_list();
            return;
        }
    }
    radio_list_draw();
}

static void radio_tick(void)
{
    if (!s_radio_local_released && !s_audio_task) {
        s_radio_local_released = true;
        s_audio_failed = false;
        s_audio_track_finished = false;
    }
    if (s_radio_exit && s_radio_local_released && !eva_radio_player_busy()) {
        lv_obj_t *screen = s_radio_scr;
        s_radio_mode = false;
        s_radio_exit = false;
        s_radio_scr = s_radio_title = s_radio_status = NULL;
        s_audio_suspend = false;
        s_audio_reset = true;
        s_last_tick = lv_tick_get();
        lv_screen_load(s_scr);
        lv_obj_delete(screen);
        update_ui();
        return;
    }
    if (s_radio_start_pending && !s_radio_exit && s_radio_local_released &&
        !eva_radio_player_busy()) {
        s_radio_start_pending = false;
        esp_err_t err = eva_radio_player_start(s_radio_station, s_volume_percent);
        s_radio_start_error = err;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "radio start failed: %s", esp_err_to_name(err));
            lv_label_set_text(s_radio_status, "启动失败，请检查网络");
            return;
        }
        const eva_radio_station_t *station;
        if (eva_radio_station_at(s_radio_station, &station) && station->id != s_radio_saved_station) {
            err = eva_radio_station_save(station->id);
            if (err == ESP_OK) s_radio_saved_station = station->id;
            else ESP_LOGW(TAG, "station save failed: %s", esp_err_to_name(err));
        }
    }
    eva_radio_player_volume(s_volume_percent);
    eva_radio_view_playing(&s_radio_view, !s_radio_exit && !s_radio_start_pending &&
        s_radio_start_error == ESP_OK && eva_radio_player_status() == EVA_RADIO_PLAYING);
    static const char *const status[] = {
        "已暂停", "正在连接电台", "正在缓冲", "正在直播",
        "信号中断，正在重连", "播放失败",
    };
    if (s_radio_exit) lv_label_set_text(s_radio_status, "正在停止，请稍候");
    else if (!s_radio_local_released) lv_label_set_text(s_radio_status, "正在切换音频");
    else if (s_radio_start_error != ESP_OK)
        lv_label_set_text(s_radio_status, s_radio_start_error == ESP_ERR_NVS_NOT_FOUND ?
            "网络未设置" : "启动失败，请检查网络");
    else lv_label_set_text(s_radio_status, status[eva_radio_player_status()]);
}

static void enter_radio_mode(void)
{
    ESP_LOGI(TAG, "radio mode requested");
    /* 本地任务自己退出并释放 I2S 后，UI tick 才能启动网络音频。 */
    eva_player_stop(&s_model);
    hide_standby_screen();
    s_audio_suspend = true;
    s_audio_playing = false;
    s_radio_mode = true;
    s_radio_exit = false;
    s_radio_start_pending = true;
    s_radio_local_released = false;
    s_radio_start_error = ESP_OK;
    s_radio_station = 0;
    uint16_t saved = 0;
    esp_err_t err = eva_radio_station_load(&saved);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "station load failed: %s", esp_err_to_name(err));
    const eva_radio_station_t *station;
    for (size_t i = 0; i < eva_radio_station_count(); i++) {
        if (eva_radio_station_at(i, &station) && station->id == saved) s_radio_station = i;
    }
    eva_radio_station_at(s_radio_station, &station);
    s_radio_saved_station = saved;
    s_radio_city = 0;
    const eva_radio_city_t *city;
    for (size_t i = 0; i < eva_radio_city_count(); i++)
        if (eva_radio_city_at(i, &city) && city->id == station->city_id) s_radio_city = i;
    eva_radio_view_create(&s_radio_view);
    eva_radio_view_battery(&s_radio_view, s_battery_bars);
    s_radio_scr = s_radio_view.screen;
    s_radio_title = s_radio_view.station;
    s_radio_status = s_radio_view.status;
    radio_selection_draw();
    lv_screen_load(s_radio_scr);
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    ESP_LOGI(TAG, "LVGL memory: total=%u used=%u peak=%u largest=%u",
        (unsigned)memory.total_size, (unsigned)(memory.total_size - memory.free_size),
        (unsigned)memory.max_used, (unsigned)memory.free_biggest_size);
}

static void enter_upload_mode(void)
{
    hide_standby_screen();
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
    lv_obj_t *battery = eva_player_frame_header(s_upload_scr, "网络热点", &eva_font_radio_16);
    eva_player_frame_battery(battery, s_battery_bars);
    upload_label(s_upload_scr, "网络设置", 57, &eva_font_radio_20, COLOR_ORANGE);
    upload_label(s_upload_scr, "音乐管理", 82, &eva_font_radio_16, COLOR_ORANGE);
    lv_obj_t *network_panel = panel(s_upload_scr, 8, 106, 224, 82,
                                    COLOR_BG, COLOR_ORANGE);
    lv_obj_t *address_panel = panel(s_upload_scr, 4, 206, 232, 46,
                                    COLOR_BG, COLOR_ORANGE);
    upload_label(network_panel, "连接开放热点", 7, &eva_font_radio_16, COLOR_ORANGE);
    esp_err_t err = eva_wifi_start();
    if (err == ESP_OK) {
        s_upload_status = upload_label(network_panel, eva_wifi_ssid(), 38,
                                       &eva_font_matisse_20, COLOR_MAGENTA);
        lv_obj_set_width(s_upload_status, 210);
        lv_obj_t *address = upload_label(address_panel, "http://192.168.4.1/", 12,
                                         &eva_font_matisse_14, COLOR_ORANGE);
        lv_obj_set_width(address, 210);
        lv_obj_set_style_text_align(address, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        ESP_LOGE(TAG, "Wi-Fi start failed: %s", esp_err_to_name(err));
        s_upload_status = upload_label(network_panel, "热点启动失败", 27,
                                       &eva_font_radio_16, COLOR_MAGENTA);
    }
    upload_label(s_upload_scr, "浏览器打开上方地址", 259, &eva_font_radio_16, COLOR_ORANGE);
    upload_label(s_upload_scr, "长按下返回 / 长按OK设置", 286, &eva_font_radio_16, COLOR_ORANGE);
    panel(s_upload_scr, 3, 313, 234, 2, COLOR_GREEN, COLOR_GREEN);
    s_upload_mode = true;
    lv_screen_load(s_upload_scr);
}

static void exit_upload_mode(void)
{
    if (eva_wifi_stop() != ESP_OK) {
        lv_obj_set_style_text_font(s_upload_status, &eva_font_radio_16, 0);
        lv_label_set_text(s_upload_status, "正在上传，请稍候");
        return;
    }
    s_upload_mode = false;
    ESP_LOGI(TAG, "upload mode stopped; restoring player");
    if (s_settings.mode == EVA_SOURCE_RADIO) s_source_request = SOURCE_REQUEST_RADIO;
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

static void screen_idle_tick(uint32_t now)
{
    if (!s_screen_off && eva_settings_screen_due(&s_settings, s_last_activity, now)) {
        bsp_display_backlight(0);
        s_screen_off = true;
        ESP_LOGI(TAG, "screen off; audio continues");
    }
}

static void process_source_request(void)
{
    source_request_t request = s_source_request;
    if (request == SOURCE_REQUEST_NONE) return;
    if (s_radio_mode) {
        if (request == SOURCE_REQUEST_RADIO && !s_radio_exit) {
            s_source_request = SOURCE_REQUEST_NONE;
            return;
        }
        s_radio_exit = true;
        s_radio_start_pending = false;
        eva_radio_player_stop();
        return;
    }
    if (request == SOURCE_REQUEST_NETWORK) {
        if (!s_upload_mode) {
            eva_player_stop(&s_model);
            enter_upload_mode();
        }
        s_source_request = SOURCE_REQUEST_NONE;
        return;
    }
    if (s_upload_mode) {
        exit_upload_mode();
        if (s_upload_mode) return; /* 正在上传时继续显示明确的等待状态。 */
    }
    s_source_request = SOURCE_REQUEST_NONE;
    if (request == SOURCE_REQUEST_RADIO) enter_radio_mode();
}

static void settings_draw(void)
{
    lv_label_set_text_fmt(s_settings_labels[EVA_SETTING_MODE], "模式    %s",
        s_settings.mode == EVA_SOURCE_RADIO ? "网络电台" : "本地播放器");
    lv_label_set_text_fmt(s_settings_labels[EVA_SETTING_VOLUME], "音量    %u%%", (unsigned)s_volume_percent);
    lv_label_set_text_fmt(s_settings_labels[EVA_SETTING_VERSION], "版本    %s", esp_app_get_description()->version);
    lv_label_set_text_fmt(s_settings_labels[EVA_SETTING_BRIGHTNESS], "亮度    %u%%", s_settings.brightness);
    if (s_settings.screen_off_seconds)
        lv_label_set_text_fmt(s_settings_labels[EVA_SETTING_SCREEN_OFF], "自动熄屏    %u 秒", s_settings.screen_off_seconds);
    else lv_label_set_text(s_settings_labels[EVA_SETTING_SCREEN_OFF], "自动熄屏    常亮");
    lv_label_set_text(s_settings_labels[EVA_SETTING_NETWORK], "网络设置    Wi-Fi配网");
    lv_label_set_text(s_settings_labels[EVA_SETTING_HELP], "操作说明");
    lv_label_set_text(s_settings_labels[EVA_SETTING_BACK], "保存并返回");
    int start = s_settings_item > 2 ? s_settings_item - 2 : 0;
    if (start > EVA_SETTING_COUNT - 5) start = EVA_SETTING_COUNT - 5;
    for (int i = 0; i < EVA_SETTING_COUNT; i++) {
        lv_obj_set_y(s_settings_rows[i], 103 + (i - start) * 36);
        bool selected = i == s_settings_item;
        lv_obj_set_style_text_color(s_settings_labels[i], lv_color_hex(COLOR_ORANGE), 0);
        lv_obj_set_style_bg_color(s_settings_rows[i], lv_color_hex(selected ? COLOR_GREEN : COLOR_BG), 0);
        lv_obj_set_style_border_color(s_settings_rows[i],
            lv_color_hex(selected && s_settings_edit ? COLOR_MAGENTA : COLOR_ORANGE), 0);
    }
    for (int i = 0; i < EVA_SETTING_COUNT; i++) {
        if (s_settings_help || i < start || i >= start + 5)
            lv_obj_add_flag(s_settings_rows[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(s_settings_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (s_settings_help) lv_obj_remove_flag(s_settings_help_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_settings_help_panel, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_settings_hint, s_settings_help ? "短按OK 返回设置" :
        s_settings_edit ? "上下调整 / OK确认" : "上下选择 / OK编辑或查看");

}

static void enter_settings_mode(void)
{
    if (s_settings_scr || (s_radio_mode && s_radio_exit)) return;
    if (s_standby_scr) {
        eva_player_stop(&s_model);
        hide_standby_screen();
    }
    s_settings_item = EVA_SETTING_MODE;
    s_settings_edit = false;
    s_settings_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_settings_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_settings_scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_pad_all(s_settings_scr, 0, 0);
    lv_obj_set_style_border_width(s_settings_scr, 0, 0);
    lv_obj_t *battery = eva_player_frame_header(s_settings_scr, "系统设置", &eva_font_radio_16);
    eva_player_frame_battery(battery, s_battery_bars);
    upload_label(s_settings_scr, "参数调整", 60, &eva_font_radio_20, COLOR_ORANGE);
    s_settings_help = false;
    for (int i = 0; i < EVA_SETTING_COUNT; i++) {
        s_settings_rows[i] = panel(s_settings_scr, 8, 103 + i * 32, 224, 30, COLOR_BG, COLOR_ORANGE);
        s_settings_labels[i] = lv_label_create(s_settings_rows[i]);
        lv_obj_set_style_text_font(s_settings_labels[i], &eva_font_radio_16, 0);
        lv_obj_set_style_text_color(s_settings_labels[i], lv_color_hex(COLOR_ORANGE), 0);
        lv_obj_set_width(s_settings_labels[i], 212);
        lv_label_set_long_mode(s_settings_labels[i], LV_LABEL_LONG_CLIP);
        lv_obj_center(s_settings_labels[i]);
    }
    s_settings_help_panel = panel(s_settings_scr, 8, 103, 224, 190, COLOR_BG, COLOR_ORANGE);
    lv_obj_t *help = lv_label_create(s_settings_help_panel);
    lv_obj_set_pos(help, 5, 5);
    lv_obj_set_width(help, 212);
    lv_obj_set_style_text_font(help, &eva_font_radio_16, 0);
    lv_obj_set_style_text_color(help, lv_color_hex(COLOR_ORANGE), 0);
    lv_label_set_text(help, "操作说明\n上下：切歌或换台\nOK：播放或暂停\n长按上：切换播放模式\n长按下：热点或返回\n电台双击OK：选择城市\n长按OK：设置或保存\n设置内调整音量与亮度\n熄屏后首键仅唤醒");
    s_settings_hint = upload_label(s_settings_scr, "", 297, &eva_font_radio_16, COLOR_ORANGE);
    settings_draw();
    lv_screen_load(s_settings_scr);
    ESP_LOGI(TAG, "settings opened");
}

static void close_settings_mode(bool network)
{
    if (s_volume_percent != s_saved_volume) {
        esp_err_t err = eva_music_store_save_volume(s_volume_percent);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "volume save failed: %s", esp_err_to_name(err));
            lv_label_set_text(s_settings_hint, "音量保存失败，请重试");
            return;
        }
        s_saved_volume = s_volume_percent;
    }
    if (memcmp(&s_settings, &s_saved_settings, sizeof(s_settings))) {
        esp_err_t err = eva_settings_save(&s_settings);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "settings save failed: %s", esp_err_to_name(err));
            lv_label_set_text(s_settings_hint, "保存失败，请重试");
            return;
        }
        s_saved_settings = s_settings;
    }
    lv_obj_t *screen = s_settings_scr;
    s_settings_scr = NULL;
    s_settings_hint = NULL;
    s_settings_help_panel = NULL;
    s_settings_help = false;
    lv_screen_load(s_upload_mode ? s_upload_scr : s_radio_mode ? s_radio_scr : s_scr);
    lv_obj_delete(screen);
    s_source_request = network ? SOURCE_REQUEST_NETWORK :
        s_upload_mode ? SOURCE_REQUEST_NONE :
        s_settings.mode == EVA_SOURCE_RADIO ? SOURCE_REQUEST_RADIO : SOURCE_REQUEST_LOCAL;
    ESP_LOGI(TAG, "settings applied: mode=%u brightness=%u timeout=%u network=%u",
        s_settings.mode, s_settings.brightness, s_settings.screen_off_seconds, network);
}

static void settings_button(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (s_settings_help) {
        if (btn == BSP_BTN_OK && (ev == BSP_BTN_CLICK || ev == BSP_BTN_LONG)) {
            s_settings_help = false;
            settings_draw();
        }
        return;
    }
    if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) { close_settings_mode(false); return; }
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_OK) {
        if (s_settings_item == EVA_SETTING_VERSION) return;
        if (s_settings_item == EVA_SETTING_HELP) { s_settings_help = true; settings_draw(); return; }
        if (s_settings_item == EVA_SETTING_NETWORK) { close_settings_mode(true); return; }
        if (s_settings_item == EVA_SETTING_BACK) { close_settings_mode(false); return; }
        s_settings_edit = !s_settings_edit;
    } else if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        int direction = btn == BSP_BTN_UP ? -1 : 1;
        if (s_settings_edit && s_settings_item == EVA_SETTING_VOLUME) {
            int volume = s_volume_percent - direction * 5;
            s_volume_percent = volume < 0 ? 0 : volume > 100 ? 100 : volume;
            if (s_radio_mode) eva_radio_player_volume(s_volume_percent);
        } else if (s_settings_edit) {
            if (eva_settings_adjust(&s_settings, s_settings_item, -direction) &&
                s_settings_item == EVA_SETTING_BRIGHTNESS)
                bsp_display_backlight(s_settings.brightness);
        } else {
            s_settings_item = (s_settings_item + EVA_SETTING_COUNT + direction) % EVA_SETTING_COUNT;
        }
    }
    settings_draw();
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
    if (s_settings.mode == EVA_SOURCE_RADIO) s_source_request = SOURCE_REQUEST_RADIO;
}

static void build_boot_screen(void)
{
    lv_obj_t *boot = create_nerv_logo_screen();
    s_boot_scr = boot;
    lv_screen_load(boot);
    lv_refr_now(lv_display_get_default());
    vTaskDelay(pdMS_TO_TICKS(40));
    bsp_display_backlight(s_settings.brightness);
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
    s_audio_task = NULL;
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
    eva_settings_defaults(&s_settings);
    esp_err_t settings_err = eva_settings_load(&s_settings);
    if (settings_err != ESP_OK && settings_err != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "settings load failed; defaults retained: %s", esp_err_to_name(settings_err));
    s_saved_settings = s_settings;
    s_settings_scr = NULL;
    s_settings_hint = NULL;
    s_screen_off = false;
    s_wake_key = false;
    s_last_activity = lv_tick_get();
    s_source_request = SOURCE_REQUEST_NONE;
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
    s_saved_volume = s_volume_percent;
    s_audio_failed = false;
    s_audio_track_finished = false;
    s_audio_suspend = false;
    s_audio_suspended = true;
    s_audio_reset = false;
    s_audio_task = NULL;
    s_upload_mode = false;
    s_radio_mode = false;
    s_radio_station = 0;
    s_radio_scr = s_radio_title = s_radio_status = NULL;
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
    s_last_activity = lv_tick_get();
    if (s_screen_off) {
        bsp_display_backlight(s_settings.brightness);
        s_screen_off = false;
        s_wake_key = true;
        ESP_LOGI(TAG, "screen awake; first key consumed");
    }
    if (s_wake_key) {
        if (ev == BSP_BTN_CLICK || ev == BSP_BTN_DOUBLE || ev == BSP_BTN_LONG) s_wake_key = false;
        return;
    }
    if (!s_ready) return;
    if (s_settings_scr) { settings_button(btn, ev); return; }
    if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        if (s_radio_list.screen) close_radio_list();
        enter_settings_mode();
        return;
    }
    if (s_radio_list.screen) { radio_list_button(btn, ev); return; }

    if (s_upload_mode) {
        if (btn == BSP_BTN_DOWN && ev == BSP_BTN_LONG) exit_upload_mode();
        return;
    }
    if (ev == BSP_BTN_LONG && btn == BSP_BTN_UP) {
        if (s_radio_mode && s_radio_exit) return;
        eva_settings_t next = s_settings;
        next.mode = s_radio_mode ? EVA_SOURCE_LOCAL : EVA_SOURCE_RADIO;
        esp_err_t err = eva_settings_save(&next);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "source save failed: %s", esp_err_to_name(err));
            enter_settings_mode();
            lv_label_set_text(s_settings_hint, "保存失败，请重试");
            return;
        }
        s_settings = s_saved_settings = next;
        s_source_request = next.mode == EVA_SOURCE_RADIO ? SOURCE_REQUEST_RADIO : SOURCE_REQUEST_LOCAL;
        return;
    }
    if (ev == BSP_BTN_LONG && btn == BSP_BTN_DOWN) {
        if (s_radio_mode && s_radio_exit) return;
        s_source_request = SOURCE_REQUEST_NETWORK;
        return;
    }
    if (s_radio_mode) {
        if (s_radio_exit) return;
        if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
            s_radio_start_pending = !eva_radio_player_busy();
            if (!s_radio_start_pending) eva_radio_player_stop();
        } else if ((btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) && ev == BSP_BTN_CLICK) {
            const eva_radio_city_t *city;
            const eva_radio_station_t *station;
            int direction = btn == BSP_BTN_UP ? -1 : 1;
            {
                eva_radio_city_at(s_radio_city, &city);
                size_t count = eva_radio_stations_for_city(city->id), index = 0;
                for (size_t i = 0; i < count; i++) {
                    const eva_radio_station_t *candidate;
                    eva_radio_station_for_city_at(city->id, i, &candidate);
                    eva_radio_station_at(s_radio_station, &station);
                    if (candidate->id == station->id) index = i;
                }
                eva_radio_station_for_city_at(city->id, (index + count + direction) % count, &station);
            }
            for (size_t i = 0; i < eva_radio_station_count(); i++) {
                const eva_radio_station_t *candidate;
                eva_radio_station_at(i, &candidate);
                if (candidate->id == station->id) s_radio_station = i;
            }
            s_radio_start_pending = true;
            eva_radio_player_stop();
            radio_selection_draw();
        } else if (btn == BSP_BTN_OK && ev == BSP_BTN_DOUBLE) {
            open_radio_list();
        }
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
