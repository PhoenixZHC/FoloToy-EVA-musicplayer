#include "eva_music_store.h"

#include <stdbool.h>
#include "audio_catalog.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wear_levelling.h"

static const char *TAG = "eva_store";
static wl_handle_t s_wl = WL_INVALID_HANDLE;
static bool s_nvs_ready;

esp_err_t eva_music_store_prepare(void)
{
    if (s_nvs_ready) return ESP_OK;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_OK) s_nvs_ready = true;
    return err;
}

esp_err_t eva_music_store_init(void)
{
    esp_err_t err = eva_music_store_prepare();
    if (err != ESP_OK) return err;

    nvs_handle_t handle;
    err = nvs_open("eva_music", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    uint8_t initialized = 0;
    err = nvs_get_u8(handle, "formatted", &initialized);
    bool first_mount = err == ESP_ERR_NVS_NOT_FOUND;
    if (err != ESP_OK && !first_mount) {
        nvs_close(handle);
        return err;
    }
    if (!first_mount && initialized != 1) {
        nvs_close(handle);
        return ESP_ERR_INVALID_STATE;
    }

    const esp_vfs_fat_mount_config_t cfg = {
        .format_if_mount_failed = first_mount,
        .max_files = 6,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };
    err = esp_vfs_fat_spiflash_mount_rw_wl("/fs", "music", &cfg, &s_wl);
    if (err == ESP_OK && first_mount) {
        err = nvs_set_u8(handle, "formatted", 1);
        if (err == ESP_OK) err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "music partition mount failed: %s", esp_err_to_name(err));
        return err;
    }
    if (audio_catalog_init() != ESP_OK) {
        ESP_LOGE(TAG, "music catalog invalid; uploads disabled");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t eva_music_store_space(uint32_t *total_bytes, uint32_t *free_bytes)
{
    uint64_t total = 0;
    uint64_t free = 0;
    esp_err_t err = esp_vfs_fat_info("/fs", &total, &free);
    if (err != ESP_OK) return err;
    if (total > UINT32_MAX || free > UINT32_MAX) return ESP_ERR_INVALID_SIZE;
    if (total_bytes) *total_bytes = (uint32_t)total;
    if (free_bytes) *free_bytes = (uint32_t)free;
    return ESP_OK;
}

esp_err_t eva_music_store_load_volume(uint8_t *percent)
{
    if (!percent) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("eva_music", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(handle, "volume", percent);
    nvs_close(handle);
    return err;
}

esp_err_t eva_music_store_save_volume(uint8_t percent)
{
    if (percent > 100) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("eva_music", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(handle, "volume", percent);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
