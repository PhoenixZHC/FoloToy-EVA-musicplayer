#include "eva_settings_store.h"
#include "nvs.h"
typedef struct { uint32_t magic; eva_settings_t value; } stored_settings_t;
/* 兼容没有屏保字段的 EVS1，保留模式、正常亮度和熄屏时间。 */
typedef struct {
    uint32_t magic;
    uint8_t mode, brightness;
    uint16_t screen_off_seconds;
} legacy_settings_t;
esp_err_t eva_settings_load(eva_settings_t *settings)
{
    if (!settings) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t err=nvs_open("eva_settings",NVS_READONLY,&nvs);
    if (err!=ESP_OK) return err;
    union { stored_settings_t current; legacy_settings_t legacy; } saved={0};
    size_t size=sizeof(saved);
    err=nvs_get_blob(nvs,"config",&saved,&size);
    nvs_close(nvs);
    if (err!=ESP_OK) return err;
    eva_settings_t value;
    if (size==sizeof(legacy_settings_t) && saved.legacy.magic==0x45565331U) {
        eva_settings_defaults(&value);
        value.mode=saved.legacy.mode;
        value.brightness=saved.legacy.brightness;
        value.screen_off_seconds=saved.legacy.screen_off_seconds;
    } else if (size==sizeof(stored_settings_t) && saved.current.magic==0x45565332U) {
        value=saved.current.value;
    } else return ESP_ERR_INVALID_ARG;
    if (!eva_settings_valid(&value)) return ESP_ERR_INVALID_ARG;
    *settings=value;
    return ESP_OK;
}
esp_err_t eva_settings_save(const eva_settings_t *settings)
{
    if (!eva_settings_valid(settings)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t err=nvs_open("eva_settings",NVS_READWRITE,&nvs);
    if (err!=ESP_OK) return err;
    stored_settings_t saved={.magic=0x45565332U,.value=*settings};
    err=nvs_set_blob(nvs,"config",&saved,sizeof(saved));
    if (err==ESP_OK) err=nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}
