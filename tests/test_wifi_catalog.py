"""Run the firmware HTTP handlers on host; mocks never enter the firmware build."""
import shutil
import subprocess
import uuid
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
STUB = r'''
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NO_MEM 0x105
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_RETURN_ON_ERROR(expr, tag, ...) do { (void)(tag); int e = (expr); if(e) return e; } while(0)
typedef void *httpd_handle_t;
typedef struct {
    size_t content_len, position;
    const unsigned char *body;
    char query[128], status[64], response[8192];
} httpd_req_t;
typedef struct { int max_uri_handlers, max_open_sockets, recv_wait_timeout, send_wait_timeout, lru_purge_enable, stack_size; } httpd_config_t;
typedef struct { const char *uri; int method; esp_err_t (*handler)(httpd_req_t *); } httpd_uri_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
#define HTTPD_SOCK_ERR_TIMEOUT -2
#define HTTP_GET 0
#define HTTP_POST 1
static inline int httpd_resp_set_status(httpd_req_t *r, const char *s) { snprintf(r->status, sizeof(r->status), "%s", s); return 0; }
static inline int httpd_resp_set_type(httpd_req_t *r, const char *s) { return 0; }
static inline int httpd_resp_set_hdr(httpd_req_t *r, const char *k, const char *v) { return 0; }
static inline int httpd_resp_send(httpd_req_t *r, const char *s, size_t n) { if(n >= sizeof(r->response)) return -1; memcpy(r->response,s,n); r->response[n]=0; return 0; }
static inline int httpd_resp_sendstr(httpd_req_t *r, const char *s) { return httpd_resp_send(r,s,strlen(s)); }
static inline int httpd_resp_send_err(httpd_req_t *r, int e, const char *s) { snprintf(r->status,sizeof(r->status),"%d",e); return httpd_resp_sendstr(r,s); }
static inline int httpd_resp_send_chunk(httpd_req_t *r, const char *s, size_t n) { size_t p=strlen(r->response); if(p+n>=sizeof(r->response))return -1; if(n)memcpy(r->response+p,s,n);r->response[p+n]=0;return 0; }
static inline int httpd_req_get_url_query_str(httpd_req_t *r,char *out,size_t cap) { if(strlen(r->query)>=cap)return -1;strcpy(out,r->query);return 0; }
static inline int httpd_query_key_value(const char *q,const char *key,char *out,size_t cap) {
    size_t k=strlen(key); while(*q){ const char *end=strchr(q,'&');size_t n=end?(size_t)(end-q):strlen(q);
    if(n>k && !strncmp(q,key,k) && q[k]=='='){n-=k+1;if(n>=cap)return -1;memcpy(out,q+k+1,n);out[n]=0;return 0;}
    if(!end)break;q=end+1;}return -1;
}
static inline int httpd_req_recv(httpd_req_t *r,char *out,size_t n) { size_t left=r->content_len-r->position;if(n>left)n=left;memcpy(out,r->body+r->position,n);r->position+=n;return (int)n; }
static inline int httpd_start(httpd_handle_t *h,const httpd_config_t *c) { *h=(void*)1;return 0; }
static inline int httpd_stop(httpd_handle_t h) { return 0; }
static inline int httpd_register_uri_handler(httpd_handle_t h,const httpd_uri_t *r) { return 0; }
typedef struct { int unused; } esp_netif_t;
static inline int esp_netif_init(void) { return 0; }
static inline int esp_event_loop_create_default(void) { return 0; }
static inline esp_netif_t *esp_netif_create_default_wifi_ap(void) { return (esp_netif_t*)1; }
static inline void esp_netif_destroy_default_wifi(esp_netif_t *n) { }
typedef struct { int unused; } wifi_init_config_t;
typedef struct { struct { unsigned char ssid[32];char password[64];int authmode,max_connection,channel,beacon_interval; } ap; } wifi_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
#define WIFI_AUTH_OPEN 0
#define WIFI_MODE_AP 0
#define WIFI_IF_AP 0
#define ESP_MAC_WIFI_STA 0
static inline int esp_wifi_init(const wifi_init_config_t *c) { return 0; }
static inline int esp_wifi_set_mode(int m) { return 0; }
static inline int esp_wifi_set_config(int m,const wifi_config_t *c) { return 0; }
static inline int esp_wifi_start(void) { return 0; }
static inline int esp_wifi_stop(void) { return 0; }
static inline int esp_wifi_deinit(void) { return 0; }
static inline int esp_read_mac(uint8_t *m,int type) { memset(m,0,6);return 0; }
static inline uint32_t esp_get_free_heap_size(void) { return 100000; }
static inline uint32_t esp_random(void) { static uint32_t value=123;return ++value; }
typedef struct { struct { unsigned magic,cf,w,h,stride; } header; const uint8_t *data;uint32_t data_size; } lv_image_dsc_t;
#define LV_IMAGE_HEADER_MAGIC 1
#define LV_COLOR_FORMAT_A8 1
'''


def test_real_wifi_handlers_reject_stale_catalog_requests():
    if not (ROOT / 'main/web_ui.h').exists() or not (ROOT / 'assets/preset_music/a002.fam').exists():
        pytest.skip('requires prepared local web assets and three factory songs')
    work = ROOT / ('build_test_wifi_' + uuid.uuid4().hex)
    work.mkdir()
    (work / 'esp_stub.h').write_text(STUB, encoding='utf-8')
    for name in ('esp_err', 'esp_http_server', 'esp_check', 'esp_log', 'esp_mac',
                 'esp_netif', 'esp_system', 'esp_random', 'esp_wifi', 'nvs_flash', 'lvgl'):
        (work / (name + '.h')).write_text('#include "esp_stub.h"\n', encoding='utf-8')
    music = work / 'music'
    shutil.copytree(ROOT / 'assets/preset_music', music)
    binary = work / 'wifi.exe'
    subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-Wno-misleading-indentation',
                    '-I' + str(work), '-I' + str(ROOT / 'main'),
                    str(ROOT / 'tests/test_wifi_catalog.c'),
                    str(ROOT / 'main/fam1_format.c'), str(ROOT / 'main/eva_title.c'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(music)], check=True)
