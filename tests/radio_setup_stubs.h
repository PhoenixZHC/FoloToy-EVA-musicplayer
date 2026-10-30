#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define HTTPD_400_BAD_REQUEST 400
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
#define HTTP_GET 0
#define HTTP_POST 1
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
typedef void *httpd_handle_t;
typedef struct { size_t content_len, position; const char *body; char response[8192]; int status; } httpd_req_t;
typedef struct { const char *uri; int method; esp_err_t (*handler)(httpd_req_t *); } httpd_uri_t;
typedef enum { WIFI_MODE_AP, WIFI_MODE_APSTA } wifi_mode_t;
typedef enum { WIFI_AUTH_OPEN, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_WPA3_PSK } wifi_auth_mode_t;
typedef struct { uint8_t ssid[33]; int8_t rssi; wifi_auth_mode_t authmode; } wifi_ap_record_t;
esp_err_t esp_wifi_get_mode(wifi_mode_t *mode);
esp_err_t esp_wifi_set_mode(wifi_mode_t mode);
esp_err_t esp_wifi_scan_start(const void *config, bool block);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *count);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count, wifi_ap_record_t *records);
esp_err_t esp_wifi_clear_ap_list(void);
unsigned esp_get_free_heap_size(void);
const char *esp_err_to_name(esp_err_t err);
int httpd_resp_set_type(httpd_req_t *req, const char *type);
int httpd_resp_set_hdr(httpd_req_t *req, const char *name, const char *value);
int httpd_resp_sendstr(httpd_req_t *req, const char *body);
int httpd_resp_send_err(httpd_req_t *req, int status, const char *body);
int httpd_req_get_hdr_value_str(httpd_req_t *req, const char *name, char *value, size_t size);
int httpd_req_recv(httpd_req_t *req, char *out, size_t size);
int httpd_register_uri_handler(httpd_handle_t server, const httpd_uri_t *route);
