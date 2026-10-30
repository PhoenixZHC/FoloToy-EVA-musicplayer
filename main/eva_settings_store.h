#pragma once
#include "eva_settings.h"
#include "esp_err.h"
esp_err_t eva_settings_load(eva_settings_t *settings);
esp_err_t eva_settings_save(const eva_settings_t *settings);
