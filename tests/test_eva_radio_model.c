#include "eva_radio_model.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    eva_radio_model_t model;
    eva_radio_model_init(&model);
    assert(model.status == EVA_RADIO_STOPPED);
    assert(!eva_radio_model_is_playing(&model));

    assert(eva_radio_model_select_station(&model, 3, -1));
    assert(model.station_index == 2);
    assert(eva_radio_model_select_station(&model, 3, 1));
    assert(model.station_index == 0);
    assert(!eva_radio_model_select_station(&model, 0, 1));

    assert(eva_radio_model_set_status(&model, EVA_RADIO_CONNECTING));
    assert(eva_radio_model_set_status(&model, EVA_RADIO_BUFFERING));
    assert(eva_radio_model_set_status(&model, EVA_RADIO_PLAYING));
    assert(eva_radio_model_is_playing(&model));
    assert(!eva_radio_model_set_status(&model, EVA_RADIO_STOPPED));
    eva_radio_model_stop(&model);
    assert(!eva_radio_model_is_playing(&model));
    assert(model.status == EVA_RADIO_STOPPED);
    assert(model.generation == 3);

    puts("radio model tests passed");
    return 0;
}
