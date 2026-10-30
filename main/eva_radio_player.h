#pragma once
#include "esp_err.h"
#include "eva_radio_model.h"
#include "esp_http_server.h"
#include <stdbool.h>
/* 调用者必须先等待本地音频任务退出。停止完成前不得启动其他音频源。 */
esp_err_t eva_radio_player_start(size_t station, uint8_t volume);
bool eva_radio_player_stop(void);
bool eva_radio_player_busy(void);
eva_radio_status_t eva_radio_player_status(void);
void eva_radio_player_volume(uint8_t volume);
esp_err_t eva_radio_setup_register(httpd_handle_t server);
