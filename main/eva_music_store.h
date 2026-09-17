#pragma once

#include <stdint.h>
#include "esp_err.h"

#define EVA_MUSIC_FREE_RESERVE (128U * 1024U)

esp_err_t eva_music_store_prepare(void);
esp_err_t eva_music_store_init(void);
esp_err_t eva_music_store_space(uint32_t *total_bytes, uint32_t *free_bytes);
esp_err_t eva_music_store_load_volume(uint8_t *percent);
esp_err_t eva_music_store_save_volume(uint8_t percent);
