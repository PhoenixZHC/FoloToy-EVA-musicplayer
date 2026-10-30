#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    EVA_RADIO_CODEC_UNKNOWN = 0,
    EVA_RADIO_CODEC_MP3,
} eva_radio_codec_t;

typedef struct {
    uint16_t id;
    const char *name;
    const char *display_name;
} eva_radio_city_t;

typedef struct {
    uint16_t id;
    uint16_t city_id;
    const char *name;
    const char *display_name;
    const char *category;
    const char *url;
    const char *source_url;
    const char *verified_on;
    uint16_t bitrate_kbps;
    eva_radio_codec_t codec;
} eva_radio_station_t;

size_t eva_radio_city_count(void);
bool eva_radio_city_at(size_t index, const eva_radio_city_t **city);
size_t eva_radio_station_count(void);
bool eva_radio_station_at(size_t index, const eva_radio_station_t **station);
size_t eva_radio_stations_for_city(uint16_t city_id);
bool eva_radio_station_for_city_at(uint16_t city_id, size_t index,
                                   const eva_radio_station_t **station);
