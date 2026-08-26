#include <assert.h>
#include "eva_player_model.h"

static void test_initial_state(void)
{
    eva_player_model_t model;
    eva_player_init(&model);

    assert(eva_player_track_index(&model) == 0);
    assert(!eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);
    assert(eva_player_current_track(&model)->title == eva_player_track_at(0)->title);
}

static void test_ok_toggles_pause_and_play(void)
{
    eva_player_model_t model;
    eva_player_init(&model);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);
    assert(eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PLAY);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);
    assert(!eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);
}

static void test_long_ok_while_paused_enters_standby(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_set_elapsed_ms(&model, 42318);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_LONG);

    assert(eva_player_is_standby(&model));
    assert(!eva_player_is_playing(&model));
    assert(eva_player_track_index(&model) == 0);
    assert(eva_player_elapsed_ms(&model) == 42318);
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);
}

static void test_standby_ignores_navigation_and_wakes_paused_on_ok_click(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_set_elapsed_ms(&model, 42318);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_LONG);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_DOWN, EVA_PLAYER_KEY_PRESS);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_DOWN, EVA_PLAYER_KEY_CLICK);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_UP, EVA_PLAYER_KEY_CLICK);

    assert(eva_player_is_standby(&model));
    assert(eva_player_track_index(&model) == 0);
    assert(eva_player_elapsed_ms(&model) == 42318);
    assert(!eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);

    assert(!eva_player_is_standby(&model));
    assert(!eva_player_is_playing(&model));
    assert(eva_player_track_index(&model) == 0);
    assert(eva_player_elapsed_ms(&model) == 42318);
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);
}

static void test_long_ok_while_playing_does_not_enter_standby(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_LONG);

    assert(!eva_player_is_standby(&model));
    assert(eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PLAY);
}

static void test_long_ok_during_end_pause_does_not_enter_standby(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);
    eva_player_finish_track(&model, 200000, 1000);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_LONG);

    assert(!eva_player_is_standby(&model));
    assert(eva_player_auto_advance_pending(&model));
    assert(!eva_player_is_playing(&model));
}

static void test_runtime_audio_events_exit_standby(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_LONG);

    eva_player_stop(&model);

    assert(!eva_player_is_standby(&model));
    assert(!eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_LONG);
    eva_player_finish_track(&model, 200000, 1000);

    assert(!eva_player_is_standby(&model));
    assert(eva_player_auto_advance_pending(&model));
}

static void test_up_press_highlights_prev_and_click_switches_previous(void)
{
    eva_player_model_t model;
    eva_player_init(&model);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_UP, EVA_PLAYER_KEY_PRESS);
    assert(eva_player_track_index(&model) == 0);
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PREV);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_UP, EVA_PLAYER_KEY_CLICK);
    assert(eva_player_track_index(&model) == 2);
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);
    assert(eva_player_elapsed_ms(&model) == 0);
}

static void test_down_press_highlights_next_and_click_switches_next(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_set_elapsed_ms(&model, 42318);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_DOWN, EVA_PLAYER_KEY_PRESS);
    assert(eva_player_track_index(&model) == 0);
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_NEXT);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_DOWN, EVA_PLAYER_KEY_CLICK);
    assert(eva_player_track_index(&model) == 1);
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);
    assert(eva_player_elapsed_ms(&model) == 0);
}

static void test_switching_while_paused_returns_to_pause_highlight(void)
{
    eva_player_model_t model;
    eva_player_init(&model);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_DOWN, EVA_PLAYER_KEY_PRESS);
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_NEXT);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_DOWN, EVA_PLAYER_KEY_CLICK);
    assert(eva_player_track_index(&model) == 1);
    assert(!eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);
}

static void test_audio_failure_returns_to_pause(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);

    eva_player_stop(&model);

    assert(!eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);
}

static void test_finished_track_waits_two_seconds_then_advances_playing(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);

    eva_player_finish_track(&model, 215432, 1000);

    assert(!eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PAUSE);
    assert(eva_player_elapsed_ms(&model) == 215432);
    assert(eva_player_auto_advance_pending(&model));
    assert(!eva_player_auto_advance_due(&model, 2999));
    assert(eva_player_auto_advance_due(&model, 3000));

    eva_player_advance_after_finish(&model);

    assert(eva_player_track_index(&model) == 1);
    assert(eva_player_elapsed_ms(&model) == 0);
    assert(eva_player_is_playing(&model));
    assert(eva_player_active_control(&model) == EVA_PLAYER_CONTROL_PLAY);
    assert(!eva_player_auto_advance_pending(&model));
}

static void test_finished_last_track_wraps_to_first(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_UP, EVA_PLAYER_KEY_CLICK);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);

    eva_player_finish_track(&model, 180000, 5000);
    eva_player_advance_after_finish(&model);

    assert(eva_player_track_index(&model) == 0);
    assert(eva_player_is_playing(&model));
}

static void test_ok_during_end_pause_starts_next_track_immediately(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);
    eva_player_finish_track(&model, 200000, 1000);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);

    assert(eva_player_track_index(&model) == 1);
    assert(eva_player_elapsed_ms(&model) == 0);
    assert(eva_player_is_playing(&model));
    assert(!eva_player_auto_advance_pending(&model));
}

static void test_manual_track_change_cancels_end_pause(void)
{
    eva_player_model_t model;
    eva_player_init(&model);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_OK, EVA_PLAYER_KEY_CLICK);
    eva_player_finish_track(&model, 200000, 1000);

    eva_player_handle_key(&model, EVA_PLAYER_KEY_DOWN, EVA_PLAYER_KEY_PRESS);
    eva_player_handle_key(&model, EVA_PLAYER_KEY_DOWN, EVA_PLAYER_KEY_CLICK);

    assert(eva_player_track_index(&model) == 1);
    assert(!eva_player_is_playing(&model));
    assert(!eva_player_auto_advance_pending(&model));
}

int main(void)
{
    test_initial_state();
    test_ok_toggles_pause_and_play();
    test_long_ok_while_paused_enters_standby();
    test_standby_ignores_navigation_and_wakes_paused_on_ok_click();
    test_long_ok_while_playing_does_not_enter_standby();
    test_long_ok_during_end_pause_does_not_enter_standby();
    test_runtime_audio_events_exit_standby();
    test_up_press_highlights_prev_and_click_switches_previous();
    test_down_press_highlights_next_and_click_switches_next();
    test_switching_while_paused_returns_to_pause_highlight();
    test_audio_failure_returns_to_pause();
    test_finished_track_waits_two_seconds_then_advances_playing();
    test_finished_last_track_wraps_to_first();
    test_ok_during_end_pause_starts_next_track_immediately();
    test_manual_track_change_cancels_end_pause();
    return 0;
}
