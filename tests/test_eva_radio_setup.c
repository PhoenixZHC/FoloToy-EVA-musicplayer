#include "radio_setup_stubs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/eva_radio_setup.c"

const char setup_page[] asm("_binary_eva_radio_setup_html_start") = "test page";
static wifi_ap_record_t aps[24];
static uint16_t ap_count;
static wifi_mode_t current_mode = WIFI_MODE_AP;
static int failure, clears, scans, saves, registrations;
static bool running = true;
static eva_radio_config_t saved;
bool eva_wifi_running(void) { return running; }
unsigned esp_get_free_heap_size(void) { return 20000; }
const char *esp_err_to_name(esp_err_t err) { return err ? "test failure" : "ok"; }
int esp_wifi_get_mode(wifi_mode_t *mode) { *mode=current_mode; return ESP_OK; }
int esp_wifi_set_mode(wifi_mode_t mode) { if(failure==3 && mode==WIFI_MODE_AP)return ESP_FAIL; current_mode=mode; return ESP_OK; }
int esp_wifi_scan_start(const void *config, bool block) { assert(block); assert(current_mode==WIFI_MODE_APSTA); scans++; return failure==1 ? ESP_FAIL : ESP_OK; }
int esp_wifi_scan_get_ap_num(uint16_t *count) { *count=ap_count; return ESP_OK; }
int esp_wifi_scan_get_ap_records(uint16_t *count,wifi_ap_record_t *records) { if(failure==2)return ESP_FAIL; if(*count>ap_count)*count=ap_count; memcpy(records,aps,*count*sizeof(*records)); return ESP_OK; }
int esp_wifi_clear_ap_list(void) { clears++; return ESP_OK; }
int httpd_resp_set_type(httpd_req_t *req,const char *type) { return ESP_OK; }
int httpd_resp_set_hdr(httpd_req_t *req,const char *name,const char *value) { return ESP_OK; }
int httpd_resp_sendstr(httpd_req_t *req,const char *body) { snprintf(req->response,sizeof(req->response),"%s",body); return ESP_OK; }
int httpd_resp_send_err(httpd_req_t *req,int status,const char *body) { req->status=status; return httpd_resp_sendstr(req,body); }
int httpd_req_get_hdr_value_str(httpd_req_t *req,const char *name,char *value,size_t size) { snprintf(value,size,"application/json"); return ESP_OK; }
int httpd_req_recv(httpd_req_t *req,char *out,size_t size) { size_t left=req->content_len-req->position; if(size>left)size=left; memcpy(out,req->body+req->position,size); req->position+=size; return (int)size; }
int httpd_register_uri_handler(httpd_handle_t server,const httpd_uri_t *route) { registrations++; return ESP_OK; }
int eva_radio_config_save(const eva_radio_config_t *config) { saves++; saved=*config; return ESP_OK; }

int main(void)
{
    aps[0]=(wifi_ap_record_t){.ssid="Home \"WiFi\"",.rssi=-30,.authmode=WIFI_AUTH_WPA2_PSK};
    aps[1]=aps[0]; aps[1].rssi=-70;
    aps[2]=(wifi_ap_record_t){.ssid="Open",.rssi=-45,.authmode=WIFI_AUTH_OPEN};
    aps[3]=(wifi_ap_record_t){.ssid="",.rssi=-50,.authmode=WIFI_AUTH_OPEN};
    aps[4]=(wifi_ap_record_t){.ssid="WPA3 only",.rssi=-55,.authmode=WIFI_AUTH_WPA3_PSK};
    aps[5]=(wifi_ap_record_t){.ssid="中文网络",.rssi=-60,.authmode=WIFI_AUTH_WPA2_PSK};
    ap_count=6;
    httpd_req_t req={0};
    assert(setup_scan_get(&req)==ESP_OK && req.status==0);
    assert(current_mode==WIFI_MODE_AP && scans==1 && clears==1);
    cJSON *json=cJSON_Parse(req.response), *list=cJSON_GetObjectItem(json,"networks");
    assert(cJSON_GetArraySize(list)==4);
    assert(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(list,0),"ssid")->valuestring,"Home \"WiFi\""));
    assert(cJSON_GetObjectItem(cJSON_GetArrayItem(list,0),"rssi")->valueint==-30);
    assert(!cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(list,1),"secure")));
    assert(!cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(list,2),"supported")));
    cJSON_Delete(json);
    for(failure=1;failure<=3;failure++) {
        current_mode=WIFI_MODE_AP; req=(httpd_req_t){0};
        setup_scan_get(&req); assert(req.status==500);
        if(failure!=3)assert(current_mode==WIFI_MODE_AP);
    }
    failure=0; current_mode=WIFI_MODE_AP; ap_count=0; req=(httpd_req_t){0};
    setup_scan_get(&req); json=cJSON_Parse(req.response);
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(json,"networks"))==0); cJSON_Delete(json);
    ap_count=24; memset(aps,0,sizeof(aps)); req=(httpd_req_t){0};
    setup_scan_get(&req); json=cJSON_Parse(req.response);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(json,"truncated"))); cJSON_Delete(json);
    running=false; req=(httpd_req_t){0}; setup_scan_get(&req); assert(req.status==500); running=true;
    const char *body="{\"ssid\":\"中文网络\",\"password\":\"12345678\"}";
    req=(httpd_req_t){.body=body,.content_len=strlen(body)}; setup_post(&req);
    assert(req.status==0 && saves==1 && !strcmp(saved.ssid,"中文网络"));
    body="{\"ssid\":\"Home\",\"password\":\"short\"}";
    req=(httpd_req_t){.body=body,.content_len=strlen(body)}; setup_post(&req); assert(req.status==400 && saves==1);
    assert(eva_radio_setup_register((void*)1)==ESP_OK && registrations==3);
    puts("radio setup: scan, cleanup, Unicode, security and save scenarios passed");
    return 0;
}
