#include "eva_settings_store.h"
#include "nvs.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char blob[64];
static size_t blob_size;
static int open_error, write_error, commit_error, closes;
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle) { assert(!strcmp(name,"eva_settings")); if(open_error)return open_error; *handle=1; return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *size) { if(!blob_size)return ESP_ERR_NVS_NOT_FOUND; assert(*size>=blob_size); memcpy(out,blob,blob_size); *size=blob_size; return ESP_OK; }
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size) { if(write_error)return write_error; assert(size<=sizeof(blob)); memcpy(blob,data,size); blob_size=size; return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t handle) { return commit_error; }
void nvs_close(nvs_handle_t handle) { closes++; }
int main(void)
{
    eva_settings_t value; eva_settings_defaults(&value);
    assert(eva_settings_load(&value)==ESP_ERR_NVS_NOT_FOUND);
    value.mode=EVA_SOURCE_RADIO; value.brightness=40; value.screen_off_seconds=30;
    assert(eva_settings_save(&value)==ESP_OK);
    eva_settings_t loaded={0}; assert(eva_settings_load(&loaded)==ESP_OK);
    assert(!memcmp(&value,&loaded,sizeof(value)));
    blob[0]^=1; assert(eva_settings_load(&loaded)==ESP_ERR_INVALID_ARG);
    blob[0]^=1; blob_size--; assert(eva_settings_load(&loaded)==ESP_ERR_INVALID_ARG); blob_size++;
    value.brightness=0; int before=closes;
    assert(eva_settings_save(&value)==ESP_ERR_INVALID_ARG && closes==before);
    value.brightness=40; open_error=ESP_FAIL; assert(eva_settings_load(&loaded)==ESP_FAIL);
    assert(eva_settings_save(&value)==ESP_FAIL); open_error=0;
    write_error=ESP_FAIL; before=closes; assert(eva_settings_save(&value)==ESP_FAIL && closes==before+1); write_error=0;
    commit_error=ESP_FAIL; assert(eva_settings_save(&value)==ESP_FAIL);
    assert(eva_settings_load(NULL)==ESP_ERR_INVALID_ARG); assert(eva_settings_save(NULL)==ESP_ERR_INVALID_ARG);
    puts("settings persistence tests passed");
    return 0;
}
