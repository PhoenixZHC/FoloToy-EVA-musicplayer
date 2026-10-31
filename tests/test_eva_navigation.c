/* 主机桩只模拟外部边界，导航和设置函数从生产源码提取。 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "eva_settings.h"
#include "eva_player_model.h"
#include "eva_radio_catalog.h"

typedef enum { BSP_BTN_UP, BSP_BTN_DOWN, BSP_BTN_OK } bsp_btn_t;
typedef enum { BSP_BTN_PRESS, BSP_BTN_CLICK, BSP_BTN_DOUBLE, BSP_BTN_LONG } bsp_btn_ev_t;
typedef enum { SOURCE_REQUEST_NONE, SOURCE_REQUEST_LOCAL, SOURCE_REQUEST_RADIO,
               SOURCE_REQUEST_NETWORK } source_request_t;
typedef int esp_err_t;
typedef int lv_obj_t;
#define ESP_OK 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
static eva_settings_t s_settings, s_saved_settings;
static eva_player_model_t s_model;
static eva_setting_item_t s_settings_item;
static source_request_t s_source_request;
static lv_obj_t screen;
static lv_obj_t *s_settings_scr, *s_settings_hint, *s_settings_help_panel;
static lv_obj_t *s_upload_scr, *s_radio_scr, *s_scr;
static struct { lv_obj_t *screen; } s_radio_list;
static bool s_settings_help, s_settings_edit, s_screen_off, s_wake_key;
static bool s_ready = true, s_upload_mode, s_radio_mode, s_radio_exit;
static bool s_radio_start_pending, s_audio_reset, busy, uploading;
static uint8_t s_volume_percent = 70, s_saved_volume = 70;
static uint32_t s_last_activity, s_last_tick;
static size_t s_radio_station, s_radio_city;
static int save_error, volume_error, persisted_mode, persisted_volume, volume_applied;
static int radio_stops, radio_starts, hotspot_starts, backlight;
static const char *hint;

static uint32_t lv_tick_get(void) { return 1234; }
static void bsp_display_backlight(int value) { backlight = value; }
static void lv_label_set_text(lv_obj_t *obj, const char *value) { (void)obj; hint = value; }
static void lv_screen_load(lv_obj_t *obj) { (void)obj; }
static void lv_obj_delete(lv_obj_t *obj) { (void)obj; }
static void settings_draw(void) {}
static esp_err_t eva_settings_save(const eva_settings_t *value)
{ if (!save_error) persisted_mode = value->mode; return save_error; }
static esp_err_t eva_music_store_save_volume(uint8_t value)
{ if (!volume_error) persisted_volume = value; return volume_error; }
static void enter_settings_mode(void) { s_settings_scr = s_settings_hint = &screen; }
static void enter_upload_mode(void) { s_upload_mode = true; hotspot_starts++; }
static void exit_upload_mode(void) { if (!uploading) s_upload_mode = false; }
static void enter_radio_mode(void) { s_radio_mode = true; radio_starts++; }
static bool eva_radio_player_busy(void) { return busy; }
static bool eva_radio_player_stop(void) { radio_stops++; return !busy; }
static void eva_radio_player_volume(uint8_t value) { volume_applied = value; }
static void open_radio_list(void) { s_radio_list.screen = &screen; }
static void close_radio_list(void) { s_radio_list.screen = NULL; }
static void radio_list_button(bsp_btn_t btn, bsp_btn_ev_t ev) { (void)btn; (void)ev; }
static void radio_selection_draw(void) {}
static void sync_standby_screen(void) {}
static void update_ui(void) {}

#include "eva_navigation_under_test.inc"

int main(void)
{
    eva_settings_defaults(&s_settings);
    s_saved_settings = s_settings;
    eva_player_init(&s_model, 3);
    eva_player_handle_key(&s_model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);
    assert(eva_player_is_playing(&s_model));
    eva_player_handle_button(BSP_BTN_DOWN, BSP_BTN_LONG);
    assert(s_source_request == SOURCE_REQUEST_NETWORK);
    process_source_request();
    assert(s_upload_mode && hotspot_starts == 1 && !eva_player_is_playing(&s_model));
    uploading = true;
    eva_player_handle_button(BSP_BTN_DOWN, BSP_BTN_LONG);
    assert(s_upload_mode);
    uploading = false;
    eva_player_handle_button(BSP_BTN_DOWN, BSP_BTN_LONG);
    assert(!s_upload_mode);

    eva_player_handle_button(BSP_BTN_UP, BSP_BTN_LONG);
    assert(s_source_request == SOURCE_REQUEST_RADIO && persisted_mode == EVA_SOURCE_RADIO);
    process_source_request();
    assert(s_radio_mode && radio_starts == 1);
    eva_player_handle_button(BSP_BTN_OK, BSP_BTN_DOUBLE);
    assert(s_radio_list.screen && radio_stops == 0);
    eva_player_handle_button(BSP_BTN_OK, BSP_BTN_LONG);
    assert(s_settings_scr && !s_radio_list.screen);
    s_settings_scr = NULL;
    eva_player_handle_button(BSP_BTN_DOWN, BSP_BTN_LONG);
    process_source_request();
    assert(s_radio_exit && radio_stops == 1 && hotspot_starts == 1);
    /* 模拟电台任务完成资源释放后，下一次 tick 才能启动 AP。 */
    s_radio_mode = s_radio_exit = false;
    process_source_request();
    assert(s_upload_mode && hotspot_starts == 2);
    enter_settings_mode();
    close_settings_mode(false);
    assert(s_upload_mode && s_source_request == SOURCE_REQUEST_NONE);
    s_upload_mode = false;

    s_radio_mode = true;
    eva_player_handle_button(BSP_BTN_UP, BSP_BTN_LONG);
    assert(s_source_request == SOURCE_REQUEST_LOCAL && persisted_mode == EVA_SOURCE_LOCAL);
    process_source_request();
    assert(s_radio_exit && radio_stops == 2 && !s_radio_start_pending);
    s_radio_exit = false;
    s_radio_mode = false;
    s_source_request = SOURCE_REQUEST_NONE;
    save_error = -1;
    eva_player_handle_button(BSP_BTN_UP, BSP_BTN_LONG);
    assert(s_settings_scr && s_settings.mode == EVA_SOURCE_LOCAL);
    assert(s_source_request == SOURCE_REQUEST_NONE && strstr(hint, "保存失败"));
    save_error = 0;

    s_settings_item = EVA_SETTING_VOLUME;
    settings_button(BSP_BTN_OK, BSP_BTN_CLICK);
    for (int i = 0; i < 30; i++) settings_button(BSP_BTN_UP, BSP_BTN_CLICK);
    assert(s_volume_percent == 100);
    s_radio_mode = true;
    for (int i = 0; i < 30; i++) settings_button(BSP_BTN_DOWN, BSP_BTN_CLICK);
    assert(s_volume_percent == 0 && volume_applied == 0);
    settings_button(BSP_BTN_OK, BSP_BTN_CLICK);
    s_settings_item = EVA_SETTING_VERSION;
    settings_button(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(!s_settings_edit);
    settings_button(BSP_BTN_DOWN, BSP_BTN_CLICK);
    assert(s_settings_item == EVA_SETTING_HELP);
    volume_error = -1;
    close_settings_mode(false);
    assert(s_settings_scr && strstr(hint, "音量保存失败"));
    volume_error = 0;
    s_settings.screen_off_seconds = 30;
    save_error = -1;
    close_settings_mode(false);
    assert(s_settings_scr && s_saved_volume == 0 && s_saved_settings.screen_off_seconds == 0);
    assert(strstr(hint, "保存失败"));
    save_error = 0;
    close_settings_mode(false);
    assert(!s_settings_scr && persisted_volume == 0 && s_saved_volume == 0);
    assert(s_saved_settings.screen_off_seconds == 30);

    s_radio_mode = false;
    s_source_request = SOURCE_REQUEST_NONE;
    s_screen_off = true;
    eva_player_handle_button(BSP_BTN_UP, BSP_BTN_PRESS);
    eva_player_handle_button(BSP_BTN_UP, BSP_BTN_LONG);
    assert(!s_screen_off && !s_wake_key && backlight == 100);
    assert(s_source_request == SOURCE_REQUEST_NONE);
    eva_player_handle_button(BSP_BTN_UP, BSP_BTN_LONG);
    assert(s_source_request == SOURCE_REQUEST_RADIO);
    return 0;
}
