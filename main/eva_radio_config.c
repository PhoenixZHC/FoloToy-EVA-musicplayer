#include "eva_radio_config.h"
#include "eva_radio_stream.h"
#include "nvs.h"
#include <string.h>

esp_err_t eva_radio_config_load(eva_radio_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    memset(config, 0, sizeof(*config));
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("eva_radio", NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;
    size_t len = sizeof(*config);
    err = nvs_get_blob(nvs, "network", config, &len);
    nvs_close(nvs);
    if (err != ESP_OK) return err;
    if (len != sizeof(*config) || !memchr(config->ssid, 0, sizeof(config->ssid)) ||
        !memchr(config->password, 0, sizeof(config->password)) ||
        !eva_radio_credentials_valid(config->ssid, config->password)) {
        memset(config, 0, sizeof(*config));
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t eva_radio_config_save(const eva_radio_config_t *config)
{
    if (!config || !memchr(config->ssid, 0, sizeof(config->ssid)) ||
        !memchr(config->password, 0, sizeof(config->password)) ||
        !eva_radio_credentials_valid(config->ssid, config->password)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("eva_radio", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(nvs, "network", config, sizeof(*config));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

esp_err_t eva_radio_station_load(uint16_t *station_id)
{
    if (!station_id) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("eva_radio", NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_get_u16(nvs, "station", station_id);
    nvs_close(nvs);
    return err;
}

esp_err_t eva_radio_station_save(uint16_t station_id)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("eva_radio", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_set_u16(nvs, "station", station_id);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}
