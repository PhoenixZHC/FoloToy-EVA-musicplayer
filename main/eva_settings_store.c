#include "eva_settings_store.h"
#include "nvs.h"
typedef struct { uint32_t magic; eva_settings_t value; } stored_settings_t;
esp_err_t eva_settings_load(eva_settings_t *settings)
{
    if (!settings) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t err=nvs_open("eva_settings",NVS_READONLY,&nvs);
    if (err!=ESP_OK) return err;
    stored_settings_t saved={0};
    size_t size=sizeof(saved);
    err=nvs_get_blob(nvs,"config",&saved,&size);
    nvs_close(nvs);
    if (err!=ESP_OK) return err;
    if (size!=sizeof(saved) || saved.magic!=0x45565331U || !eva_settings_valid(&saved.value))
        return ESP_ERR_INVALID_ARG;
    *settings=saved.value;
    return ESP_OK;
}
esp_err_t eva_settings_save(const eva_settings_t *settings)
{
    if (!eva_settings_valid(settings)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t err=nvs_open("eva_settings",NVS_READWRITE,&nvs);
    if (err!=ESP_OK) return err;
    stored_settings_t saved={.magic=0x45565331U,.value=*settings};
    err=nvs_set_blob(nvs,"config",&saved,sizeof(saved));
    if (err==ESP_OK) err=nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}
