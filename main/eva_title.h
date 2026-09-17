#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"

#define EVA_TITLE_MAX_W 768U
#define EVA_TITLE_H 34U
#define EVA_TITLE_MAX_BYTES (8U + EVA_TITLE_MAX_W * EVA_TITLE_H)

bool eva_title_load(uint32_t index, const lv_image_dsc_t **image);
bool eva_title_valid_file(const char *path, size_t expected_size);
bool eva_title_path(uint32_t index, char *out, size_t cap);
