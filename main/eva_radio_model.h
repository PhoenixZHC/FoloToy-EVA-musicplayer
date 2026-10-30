#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    EVA_RADIO_STOPPED = 0,
    EVA_RADIO_CONNECTING,
    EVA_RADIO_BUFFERING,
    EVA_RADIO_PLAYING,
    EVA_RADIO_RECONNECTING,
    EVA_RADIO_FAILED,
} eva_radio_status_t;

typedef struct {
    size_t city_index;
    size_t station_index;
    eva_radio_status_t status;
    uint32_t generation;
} eva_radio_model_t;

void eva_radio_model_init(eva_radio_model_t *model);
bool eva_radio_model_select_city(eva_radio_model_t *model, size_t city_index,
                                 size_t station_count);
bool eva_radio_model_select_station(eva_radio_model_t *model, size_t station_count,
                                    int direction);
bool eva_radio_model_set_status(eva_radio_model_t *model, eva_radio_status_t status);
void eva_radio_model_stop(eva_radio_model_t *model);
bool eva_radio_model_is_playing(const eva_radio_model_t *model);
