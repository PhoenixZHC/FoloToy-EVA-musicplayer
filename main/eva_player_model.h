#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    EVA_PLAYER_KEY_UP = 0,
    EVA_PLAYER_KEY_DOWN,
    EVA_PLAYER_KEY_OK,
} eva_player_key_t;

typedef enum {
    EVA_PLAYER_KEY_PRESS = 0,
    EVA_PLAYER_KEY_CLICK,
    EVA_PLAYER_KEY_LONG,
} eva_player_key_event_t;

typedef enum {
    EVA_PLAYER_CONTROL_PREV = 0,
    EVA_PLAYER_CONTROL_PLAY,
    EVA_PLAYER_CONTROL_NEXT,
    EVA_PLAYER_CONTROL_PAUSE,
} eva_player_control_t;

typedef struct {
    size_t track_index;
    size_t track_count;
    bool playing;
    bool auto_advance_pending;
    bool standby;
    eva_player_control_t active_control;
    uint32_t elapsed_ms;
    uint32_t auto_advance_at_ms;
} eva_player_model_t;

void eva_player_init(eva_player_model_t *model, size_t track_count);
void eva_player_set_track_count(eva_player_model_t *model, size_t track_count);
void eva_player_handle_key(eva_player_model_t *model, eva_player_key_t key,
                           eva_player_key_event_t event);
void eva_player_stop(eva_player_model_t *model);
void eva_player_finish_track(eva_player_model_t *model, uint32_t duration_ms,
                             uint32_t now_ms);
bool eva_player_auto_advance_pending(const eva_player_model_t *model);
bool eva_player_auto_advance_due(const eva_player_model_t *model, uint32_t now_ms);
void eva_player_advance_after_finish(eva_player_model_t *model);
void eva_player_set_elapsed_ms(eva_player_model_t *model, uint32_t elapsed_ms);
uint32_t eva_player_elapsed_ms(const eva_player_model_t *model);
size_t eva_player_track_index(const eva_player_model_t *model);
bool eva_player_is_playing(const eva_player_model_t *model);
bool eva_player_is_standby(const eva_player_model_t *model);
eva_player_control_t eva_player_active_control(const eva_player_model_t *model);
size_t eva_player_track_count(const eva_player_model_t *model);
