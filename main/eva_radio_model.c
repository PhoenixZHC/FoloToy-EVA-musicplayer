#include "eva_radio_model.h"

#include "eva_radio_catalog.h"

void eva_radio_model_init(eva_radio_model_t *model)
{
    if (!model) return;
    *model = (eva_radio_model_t){ .status = EVA_RADIO_STOPPED };
}

bool eva_radio_model_select_city(eva_radio_model_t *model, size_t city_index,
                                 size_t station_count)
{
    if (!model || city_index >= eva_radio_city_count() || station_count == 0)
        return false;
    model->city_index = city_index;
    model->station_index = 0;
    model->generation++;
    model->status = EVA_RADIO_STOPPED;
    return true;
}

bool eva_radio_model_select_station(eva_radio_model_t *model, size_t station_count,
                                    int direction)
{
    if (!model || station_count == 0 || direction == 0) return false;
    if (direction < 0)
        model->station_index = (model->station_index + station_count - 1U) % station_count;
    else
        model->station_index = (model->station_index + 1U) % station_count;
    model->generation++;
    model->status = EVA_RADIO_STOPPED;
    return true;
}

bool eva_radio_model_set_status(eva_radio_model_t *model, eva_radio_status_t status)
{
    if (!model || status <= EVA_RADIO_STOPPED || status > EVA_RADIO_FAILED) return false;
    model->status = status;
    return true;
}

void eva_radio_model_stop(eva_radio_model_t *model)
{
    if (!model) return;
    model->generation++;
    model->status = EVA_RADIO_STOPPED;
}

bool eva_radio_model_is_playing(const eva_radio_model_t *model)
{
    return model && model->status == EVA_RADIO_PLAYING;
}
