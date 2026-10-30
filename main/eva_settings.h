#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { EVA_SOURCE_LOCAL, EVA_SOURCE_RADIO } eva_source_t;
typedef enum { EVA_SETTING_MODE, EVA_SETTING_BRIGHTNESS, EVA_SETTING_SCREEN_OFF,
               EVA_SETTING_NETWORK, EVA_SETTING_HELP, EVA_SETTING_BACK, EVA_SETTING_COUNT } eva_setting_item_t;
typedef struct { uint8_t mode, brightness; uint16_t screen_off_seconds; } eva_settings_t;
void eva_settings_defaults(eva_settings_t *settings);
bool eva_settings_valid(const eva_settings_t *settings);
bool eva_settings_adjust(eva_settings_t *settings, eva_setting_item_t item, int direction);
bool eva_settings_screen_due(const eva_settings_t *settings, uint32_t last_activity, uint32_t now);
