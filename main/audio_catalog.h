#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef ESP_PLATFORM
#include "esp_err.h"
#else
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NO_MEM 0x105
#endif

#define AUDIO_MAX_TRACKS 32u
#define AUDIO_PATH_MAX 192u
#define AUDIO_TEMP_NAME "audio.tmp"
#define AUDIO_ROOT_PATH "/fs"

typedef struct {
    uint32_t size;
    uint16_t sample_rate;
    uint32_t sample_count;
    uint32_t duration_ms;
    char title[65];
} audio_track_t;

typedef enum {
    AUDIO_CATALOG_OK = 0,
    AUDIO_CATALOG_PATH,
    AUDIO_CATALOG_GAP,
    AUDIO_CATALOG_INVALID_FILE,
    AUDIO_CATALOG_IO,
} audio_catalog_error_t;

typedef struct {
    char root[AUDIO_PATH_MAX];
    audio_track_t items[AUDIO_MAX_TRACKS];
    uint32_t count;
    uint32_t layout_generation;
    audio_catalog_error_t error;
    uint32_t error_index;
    bool healthy;
} audio_catalog_t;

bool audio_catalog_scan_path(audio_catalog_t *catalog, const char *root);
bool audio_catalog_name_path(const audio_catalog_t *catalog, uint32_t index,
                             char *out, size_t out_size);
bool audio_catalog_temp_name_path(const audio_catalog_t *catalog,
                                  char *out, size_t out_size);
bool audio_catalog_commit_temp_path(audio_catalog_t *catalog, uint32_t expected_size,
                                    audio_track_t *committed);
bool audio_catalog_delete_path(audio_catalog_t *catalog, uint32_t index);

esp_err_t audio_catalog_init(void);
uint32_t audio_catalog_count(void);
bool audio_catalog_healthy(void);
uint32_t audio_catalog_generation(void);
uint32_t audio_catalog_snapshot(audio_track_t *items, uint32_t capacity,
                                uint32_t *generation, bool *healthy);
bool audio_catalog_get(uint32_t index, audio_track_t *out);
esp_err_t audio_catalog_start_add(uint32_t expected_size, FILE **file);
esp_err_t audio_catalog_finish_add(void);
esp_err_t audio_catalog_abort_add(FILE *file);
esp_err_t audio_catalog_delete(uint32_t index);
bool audio_catalog_read_at(uint32_t index, uint32_t expected_generation,
                           uint32_t offset, void *out, size_t len);
