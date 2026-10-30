#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef struct {
    char ssid[33];
    char password[64];
} eva_radio_config_t;
esp_err_t eva_radio_config_load(eva_radio_config_t *config);
esp_err_t eva_radio_config_save(const eva_radio_config_t *config);
esp_err_t eva_radio_station_load(uint16_t *station_id);
esp_err_t eva_radio_station_save(uint16_t station_id);
