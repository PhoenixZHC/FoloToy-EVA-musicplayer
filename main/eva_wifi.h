#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t eva_wifi_start(void);
esp_err_t eva_wifi_stop(void);
bool eva_wifi_running(void);
const char *eva_wifi_ssid(void);
