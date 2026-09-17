#include "eva_player_model.h"

#define AUTO_ADVANCE_DELAY_MS 2000U

static eva_player_control_t playback_control(const eva_player_model_t *model)
{
    return model->playing ? EVA_PLAYER_CONTROL_PLAY : EVA_PLAYER_CONTROL_PAUSE;
}

void eva_player_init(eva_player_model_t *model, size_t track_count)
{
    model->track_index = 0;
    model->track_count = track_count;
    model->playing = false;
    model->auto_advance_pending = false;
    model->standby = false;
    model->active_control = EVA_PLAYER_CONTROL_PAUSE;
    model->elapsed_ms = 0;
    model->auto_advance_at_ms = 0;
}

void eva_player_set_track_count(eva_player_model_t *model, size_t track_count)
{
    model->track_count = track_count;
    if (track_count == 0) {
        model->track_index = 0;
        eva_player_stop(model);
        model->elapsed_ms = 0;
    } else if (model->track_index >= track_count) {
        model->track_index = track_count - 1U;
        model->elapsed_ms = 0;
    }
}

void eva_player_handle_key(eva_player_model_t *model, eva_player_key_t key,
                           eva_player_key_event_t event)
{
    if (model->standby) {
        if (event == EVA_PLAYER_KEY_CLICK && key == EVA_PLAYER_KEY_OK) {
            model->standby = false;
        }
        return;
    }

    if (model->auto_advance_pending) {
        if (event == EVA_PLAYER_KEY_CLICK && key == EVA_PLAYER_KEY_OK) {
            eva_player_advance_after_finish(model);
            return;
        }
        if (key == EVA_PLAYER_KEY_UP || key == EVA_PLAYER_KEY_DOWN) {
            model->auto_advance_pending = false;
        }
    }

    if (event == EVA_PLAYER_KEY_LONG) {
        if (key == EVA_PLAYER_KEY_OK && !model->playing && !model->auto_advance_pending) {
            model->standby = true;
        }
        return;
    }

    if (event == EVA_PLAYER_KEY_PRESS) {
        if (key == EVA_PLAYER_KEY_UP) {
            model->active_control = EVA_PLAYER_CONTROL_PREV;
        } else if (key == EVA_PLAYER_KEY_DOWN) {
            model->active_control = EVA_PLAYER_CONTROL_NEXT;
        }
        return;
    }

    if (event != EVA_PLAYER_KEY_CLICK) {
        return;
    }

    if (key == EVA_PLAYER_KEY_OK) {
        if (model->track_count == 0) return;
        model->playing = !model->playing;
        model->active_control = playback_control(model);
        return;
    }

    if (key == EVA_PLAYER_KEY_UP) {
        if (model->track_count == 0) return;
        model->track_index = (model->track_index + model->track_count - 1U) %
                             model->track_count;
        model->elapsed_ms = 0;
        model->active_control = playback_control(model);
        return;
    }

    if (key == EVA_PLAYER_KEY_DOWN) {
        if (model->track_count == 0) return;
        model->track_index = (model->track_index + 1U) % model->track_count;
        model->elapsed_ms = 0;
        model->active_control = playback_control(model);
    }
}

void eva_player_stop(eva_player_model_t *model)
{
    model->playing = false;
    model->auto_advance_pending = false;
    model->standby = false;
    model->active_control = EVA_PLAYER_CONTROL_PAUSE;
}

void eva_player_finish_track(eva_player_model_t *model, uint32_t duration_ms,
                             uint32_t now_ms)
{
    model->playing = false;
    model->auto_advance_pending = true;
    model->standby = false;
    model->active_control = EVA_PLAYER_CONTROL_PAUSE;
    model->elapsed_ms = duration_ms;
    model->auto_advance_at_ms = now_ms + AUTO_ADVANCE_DELAY_MS;
}

bool eva_player_auto_advance_pending(const eva_player_model_t *model)
{
    return model->auto_advance_pending;
}

bool eva_player_auto_advance_due(const eva_player_model_t *model, uint32_t now_ms)
{
    return model->auto_advance_pending &&
           (int32_t)(now_ms - model->auto_advance_at_ms) >= 0;
}

void eva_player_advance_after_finish(eva_player_model_t *model)
{
    if (model->track_count == 0) {
        eva_player_stop(model);
        return;
    }
    model->track_index = (model->track_index + 1U) % model->track_count;
    model->playing = true;
    model->auto_advance_pending = false;
    model->standby = false;
    model->active_control = EVA_PLAYER_CONTROL_PLAY;
    model->elapsed_ms = 0;
}

void eva_player_set_elapsed_ms(eva_player_model_t *model, uint32_t elapsed_ms)
{
    model->elapsed_ms = elapsed_ms;
}

uint32_t eva_player_elapsed_ms(const eva_player_model_t *model)
{
    return model->elapsed_ms;
}

size_t eva_player_track_index(const eva_player_model_t *model)
{
    return model->track_index;
}

bool eva_player_is_playing(const eva_player_model_t *model)
{
    return model->playing;
}

bool eva_player_is_standby(const eva_player_model_t *model)
{
    return model->standby;
}

eva_player_control_t eva_player_active_control(const eva_player_model_t *model)
{
    return model->active_control;
}

size_t eva_player_track_count(const eva_player_model_t *model)
{
    return model->track_count;
}
