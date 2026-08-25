#include "eva_player.h"
#include "eva_player_model.h"
#include "eva_adpcm.h"
#include "eva_clock.h"
#include "eva_track_layout.h"
#include "eva_logo_assets.h"
#include "eva_text_assets.h"
#include "eva_text_buttons.h"
#include "bsp_audio.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdlib.h>

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

static const char *TAG = "eva_player";

static lv_obj_t *s_scr;
static lv_obj_t *s_boot_scr;
static lv_obj_t *s_clock;
static lv_obj_t *s_track_image;
static lv_obj_t *s_track_image_follow;
static lv_obj_t *s_buttons[4];
static lv_timer_t *s_ui_timer;
static lv_timer_t *s_boot_timer;
static uint32_t s_last_tick;
static uint32_t s_last_drawn_elapsed = UINT32_MAX;
static size_t s_displayed_track = SIZE_MAX;
static int s_track_image_width;
static eva_player_model_t s_model;
static bool s_ready;
static bool s_audio_ready;
static TaskHandle_t s_audio_task;
static volatile bool s_audio_playing;
static volatile size_t s_audio_track;
static volatile bool s_audio_failed;
static volatile bool s_audio_track_finished;
static volatile size_t s_audio_finished_track;
static volatile uint32_t s_audio_finished_duration_ms;

LV_DRAW_BUF_DEFINE_STATIC(s_clock_draw_buf, 224, 82, LV_COLOR_FORMAT_A8);

extern const uint8_t s_track0_adpcm_start[] asm("_binary_track0_adpcm_start");
extern const uint8_t s_track0_adpcm_end[] asm("_binary_track0_adpcm_end");
extern const uint8_t s_track1_adpcm_start[] asm("_binary_track1_adpcm_start");
extern const uint8_t s_track1_adpcm_end[] asm("_binary_track1_adpcm_end");
extern const uint8_t s_track2_adpcm_start[] asm("_binary_track2_adpcm_start");
extern const uint8_t s_track2_adpcm_end[] asm("_binary_track2_adpcm_end");

static const lv_image_dsc_t *TRACK_IMAGES[] = {
    &eva_text_track_0,
    &eva_text_track_1,
    &eva_text_track_2,
};

typedef struct {
    const uint8_t *start;
    const uint8_t *end;
} audio_asset_t;

static const audio_asset_t AUDIO_ASSETS[] = {
    { s_track0_adpcm_start, s_track0_adpcm_end },
    { s_track1_adpcm_start, s_track1_adpcm_end },
    { s_track2_adpcm_start, s_track2_adpcm_end },
};

static void audio_task(void *arg);

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

    const lv_image_dsc_t *image = TRACK_IMAGES[track];
    lv_anim_delete(s_track_image, track_scroll_cb);
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

static bool reset_track_decoder(eva_adpcm_decoder_t *decoder, size_t track)
{
    if (track >= sizeof(AUDIO_ASSETS) / sizeof(AUDIO_ASSETS[0])) return false;
    const uint8_t *start = AUDIO_ASSETS[track].start;
    const uint8_t *end = AUDIO_ASSETS[track].end;
    return eva_adpcm_reset(decoder, start, end, SAMPLE_RATE);
}

static void update_button_styles(void)
{
    eva_player_control_t active = eva_player_active_control(&s_model);
    for (int i = 0; i < 4; i++) {
        uint32_t bg = active == (eva_player_control_t)i ? COLOR_MAGENTA : COLOR_BG;
        lv_obj_set_style_bg_color(s_buttons[i], lv_color_hex(bg), 0);
    }
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
    if (s_audio_playing && !s_audio_ready) {
        ESP_LOGE(TAG, "audio unavailable, playback stopped");
        eva_player_stop(&s_model);
        s_audio_playing = false;
        return true;
    }
    if (s_audio_playing && s_audio_ready && !s_audio_task) {
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
        ESP_LOGI(TAG, "auto advance: track=%u",
                 (unsigned)eva_player_track_index(&s_model));
        delta = 0;
        state_changed = true;
    }
    state_changed = sync_audio_snapshot() || state_changed;
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
    lv_obj_t *internal_img = lv_image_create(internal_panel);
    lv_image_set_src(internal_img, &eva_text_internal_jp);
    lv_obj_center(internal_img);

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

    panel(s_scr, 3, 258, 234, 45, COLOR_GREEN, COLOR_ORANGE);
    make_button(s_scr, EVA_PLAYER_CONTROL_PREV, &eva_text_btn_prev, 5);
    make_button(s_scr, EVA_PLAYER_CONTROL_PLAY, &eva_text_btn_play, 64);
    make_button(s_scr, EVA_PLAYER_CONTROL_NEXT, &eva_text_btn_next, 123);
    make_button(s_scr, EVA_PLAYER_CONTROL_PAUSE, &eva_text_btn_pause, 182);
    panel(s_scr, 3, 313, 234, 2, COLOR_GREEN, COLOR_GREEN);

    eva_player_init(&s_model);
    s_displayed_track = SIZE_MAX;
    s_last_tick = lv_tick_get();
    s_ready = true;
    update_ui();
    s_ui_timer = lv_timer_create(ui_tick, UI_TICK_MS, NULL);
    lv_screen_load(s_scr);
}

static void boot_done(lv_timer_t *timer)
{
    if (timer) {
        lv_timer_delete(timer);
        s_boot_timer = NULL;
    }
    lv_obj_t *boot = s_boot_scr;
    build_player_screen();
    if (boot) {
        lv_obj_delete(boot);
        s_boot_scr = NULL;
    }
}

static void build_boot_screen(void)
{
    lv_obj_t *boot = lv_obj_create(NULL);
    s_boot_scr = boot;
    lv_obj_remove_flag(boot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(boot, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(boot, 0, 0);
    lv_obj_set_style_pad_all(boot, 0, 0);

    lv_obj_t *logo = lv_image_create(boot);
    lv_image_set_src(logo, &eva_logo_nerv);
    lv_obj_center(logo);

    lv_screen_load(boot);
    s_boot_timer = lv_timer_create(boot_done, 3000, NULL);
}

static void audio_task(void *arg)
{
    (void)arg;
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "audio format init failed");
        s_audio_failed = true;
        vTaskDelete(NULL);
        return;
    }
    bsp_audio_set_volume(70);

    int16_t *buf = malloc(AUDIO_CHUNK * sizeof(int16_t));
    if (!buf) {
        ESP_LOGE(TAG, "audio buffer alloc failed");
        s_audio_failed = true;
        vTaskDelete(NULL);
        return;
    }

    eva_adpcm_decoder_t decoder = { 0 };
    size_t current_track = (size_t)-1;
    for (;;) {
        if (!s_audio_playing) {
            vTaskDelay(pdMS_TO_TICKS(30));
            continue;
        }

        size_t track = s_audio_track;
        if (track != current_track) {
            current_track = track;
            if (!reset_track_decoder(&decoder, current_track)) {
                ESP_LOGE(TAG, "audio asset invalid: track=%u", (unsigned)current_track);
                break;
            }
        }

        bool track_completed = false;
        for (int i = 0; i < AUDIO_CHUNK; i++) {
            if (eva_adpcm_finished(&decoder)) {
                for (int j = i; j < AUDIO_CHUNK; j++) {
                    buf[j] = 0;
                }
                track_completed = true;
                break;
            }
            buf[i] = eva_adpcm_next(&decoder);
        }
        if (eva_adpcm_finished(&decoder)) track_completed = true;
        if (bsp_audio_write(buf, AUDIO_CHUNK * sizeof(int16_t)) != ESP_OK) {
            ESP_LOGE(TAG, "audio write failed");
            goto audio_failed;
        }
        if (track_completed) {
            uint32_t duration_ms = (uint32_t)(((uint64_t)eva_adpcm_sample_count(&decoder) *
                                               1000U) / SAMPLE_RATE);
            s_audio_finished_track = current_track;
            s_audio_finished_duration_ms = duration_ms;
            s_audio_track_finished = true;
            s_audio_playing = false;
        }
    }

audio_failed:
    free(buf);
    s_audio_failed = true;
    vTaskDelete(NULL);
}

void eva_player_start(bool audio_ready)
{
    s_audio_ready = audio_ready;
    s_audio_failed = false;
    s_audio_track_finished = false;
    s_ready = false;
    build_boot_screen();

}

void eva_player_handle_button(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!s_ready) return;

    eva_player_key_t key;
    if (btn == BSP_BTN_UP) key = EVA_PLAYER_KEY_UP;
    else if (btn == BSP_BTN_DOWN) key = EVA_PLAYER_KEY_DOWN;
    else if (btn == BSP_BTN_OK) key = EVA_PLAYER_KEY_OK;
    else return;

    eva_player_key_event_t event;
    if (ev == BSP_BTN_PRESS) event = EVA_PLAYER_KEY_PRESS;
    else if (ev == BSP_BTN_CLICK) event = EVA_PLAYER_KEY_CLICK;
    else return;

    bool was_auto_advance_pending = eva_player_auto_advance_pending(&s_model);
    eva_player_handle_key(&s_model, key, event);
    if (was_auto_advance_pending ||
        (event == EVA_PLAYER_KEY_CLICK &&
         (key == EVA_PLAYER_KEY_UP || key == EVA_PLAYER_KEY_DOWN))) {
        s_last_tick = lv_tick_get();
    }
    update_ui();
}
