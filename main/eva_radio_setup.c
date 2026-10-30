#include "eva_radio_player.h"
#include "eva_radio_config.h"
#include "eva_radio_stream.h"
#include "eva_wifi.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

/* 配网页扫描短暂启用 STA，但不连接路由器；结束后恢复纯 AP，释放扫描结果。 */
static esp_err_t setup_scan_get(httpd_req_t *req)
{
    enum { MAX_NETWORKS = 20 };
    wifi_mode_t mode;
    if (!eva_wifi_running() || esp_wifi_get_mode(&mode) != ESP_OK || mode != WIFI_MODE_AP)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "hotspot not ready");
    wifi_ap_record_t *records = calloc(MAX_NETWORKS, sizeof(*records));
    if (!records)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "not enough memory to scan");
    uint16_t count = MAX_NETWORKS, total = 0;
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_scan_start(NULL, true);
    if (err == ESP_OK) err = esp_wifi_scan_get_ap_num(&total);
    if (err == ESP_OK) err = esp_wifi_scan_get_ap_records(&count, records);
    /* 扫描失败或读取结果失败时也必须释放驱动的 AP 列表。 */
    esp_wifi_clear_ap_list();
    esp_err_t restore = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK || restore != ESP_OK) {
        ESP_LOGW("eva_setup", "scan failed: %s; AP restore: %s",
                 esp_err_to_name(err), esp_err_to_name(restore));
        free(records);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Wi-Fi scan failed; try again");
    }
    cJSON *json = cJSON_CreateObject();
    cJSON *networks = json ? cJSON_AddArrayToObject(json, "networks") : NULL;
    bool ok = networks && cJSON_AddBoolToObject(json, "truncated", total > MAX_NETWORKS);
    for (uint16_t i = 0; ok && i < count; i++) {
        records[i].ssid[sizeof(records[i].ssid) - 1] = 0;
        if (!records[i].ssid[0]) continue;
        bool duplicate = false;
        for (uint16_t j = 0; j < i; j++)
            if (!strcmp((const char *)records[i].ssid, (const char *)records[j].ssid)) duplicate = true;
        if (duplicate) continue;
        wifi_auth_mode_t auth = records[i].authmode;
        bool supported = auth == WIFI_AUTH_OPEN || auth == WIFI_AUTH_WPA_PSK ||
            auth == WIFI_AUTH_WPA2_PSK || auth == WIFI_AUTH_WPA_WPA2_PSK ||
            auth == WIFI_AUTH_WPA2_WPA3_PSK;
        cJSON *item = cJSON_CreateObject();
        if (!item) { ok = false; break; }
        ok = cJSON_AddStringToObject(item, "ssid", (const char *)records[i].ssid) &&
             cJSON_AddNumberToObject(item, "rssi", records[i].rssi) &&
             cJSON_AddBoolToObject(item, "secure", auth != WIFI_AUTH_OPEN) &&
             cJSON_AddBoolToObject(item, "supported", supported);
        if (!ok || !cJSON_AddItemToArray(networks, item)) {
            cJSON_Delete(item);
            ok = false;
        }
    }
    free(records);
    char *body = ok ? cJSON_PrintUnformatted(json) : NULL;
    ESP_LOGI("eva_setup", "scan: found=%u shown=%d heap=%u", total,
             networks ? cJSON_GetArraySize(networks) : 0, (unsigned)esp_get_free_heap_size());
    cJSON_Delete(json);
    if (!body)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "not enough memory for scan results");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    err = httpd_resp_sendstr(req, body);
    cJSON_free(body);
    return err;
}

static esp_err_t setup_get(httpd_req_t *req)
{
    extern const char page[] asm("_binary_eva_radio_setup_html_start");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, page);
}
static esp_err_t setup_post(httpd_req_t *req)
{
    char content_type[48];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK ||
        strcmp(content_type, "application/json") != 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "application/json required");
    char body[512];
    if (req->content_len == 0 || req->content_len >= sizeof(body))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid size");
    size_t got = 0;
    while (got < req->content_len) {
        int n = httpd_req_recv(req, body + got, req->content_len - got);
        if (n <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "incomplete request");
        got += n;
    }
    body[got] = 0;
    cJSON *json = cJSON_Parse(body);
    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password");
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (cJSON_IsString(ssid) && cJSON_IsString(password) &&
        eva_radio_credentials_valid(ssid->valuestring, password->valuestring)) {
        eva_radio_config_t config = {0};
        memcpy(config.ssid, ssid->valuestring, strlen(ssid->valuestring));
        memcpy(config.password, password->valuestring, strlen(password->valuestring));
        err = eva_radio_config_save(&config);
        memset(&config, 0, sizeof(config));
    }
    cJSON_Delete(json);
    memset(body, 0, sizeof(body));
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (err != ESP_OK) return httpd_resp_send_err(req,
        err == ESP_ERR_INVALID_ARG ? HTTPD_400_BAD_REQUEST : HTTPD_500_INTERNAL_SERVER_ERROR,
        err == ESP_ERR_INVALID_ARG ? "invalid SSID or password" : "settings save failed");
    return httpd_resp_sendstr(req, "ok");
}
esp_err_t eva_radio_setup_register(httpd_handle_t server)
{
    const httpd_uri_t get = {.uri="/radio/setup", .method=HTTP_GET, .handler=setup_get};
    const httpd_uri_t scan = {.uri="/radio/scan", .method=HTTP_GET, .handler=setup_scan_get};
    const httpd_uri_t post = {.uri="/radio/network", .method=HTTP_POST, .handler=setup_post};
    esp_err_t err = httpd_register_uri_handler(server, &get);
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &scan);
    return err == ESP_OK ? httpd_register_uri_handler(server, &post) : err;
}
