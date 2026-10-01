#include "eva_wifi.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "audio_catalog.h"
#include "eva_music_store.h"
#include "eva_title.h"
#include "fam1_format.h"
#include "web_ui.h"
#include "esp_http_server.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "eva_wifi";
static httpd_handle_t s_server;
static esp_netif_t *s_ap;
static bool s_netif_ready;
static bool s_wifi_ready;
static volatile bool s_upload_busy;
static char s_ssid[32] = "EVA-PLAYER";
static char s_session[17];

static void catalog_revision(char *out, size_t size)
{
    snprintf(out, size, "%s-%lu", s_session,
             (unsigned long)audio_catalog_generation());
}

/* Requests are handled serially by HTTPD. Bind the visible index to the
   exact catalog and AP session from which the browser obtained it. */
static bool current_track_query(httpd_req_t *req, uint32_t *index)
{
    char query[128], value[16], received[40], current[40];
    if (!audio_catalog_healthy() ||
        httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "i", value, sizeof(value)) != ESP_OK ||
        httpd_query_key_value(query, "r", received, sizeof(received)) != ESP_OK)
        return false;
    catalog_revision(current, sizeof(current));
    if (strcmp(received, current) != 0 || value[0] < '0' || value[0] > '9')
        return false;
    char *end;
    errno = 0;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno || *end || parsed >= audio_catalog_count()) return false;
    *index = (uint32_t)parsed;
    return true;
}

bool eva_wifi_running(void) { return s_server != NULL; }
const char *eva_wifi_ssid(void) { return s_ssid; }

static esp_err_t send_status(httpd_req_t *req, const char *status, const char *message)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_sendstr(req, message);
}

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)WEB_UI_HTML, WEB_UI_HTML_LEN);
}

static bool json_escape(char *out, size_t cap, const char *in)
{
    size_t pos = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        if (*p == '"' || *p == '\\') {
            if (pos + 2 >= cap) return false;
            out[pos++] = '\\';
            out[pos++] = (char)*p;
        } else if (*p < 0x20 || *p == 0x7f) {
            if (pos + 1 >= cap) return false;
            out[pos++] = ' ';
        } else {
            if (pos + 1 >= cap) return false;
            out[pos++] = (char)*p;
        }
    }
    out[pos] = '\0';
    return true;
}

static esp_err_t list_get(httpd_req_t *req)
{
    audio_track_t tracks[AUDIO_MAX_TRACKS];
    bool healthy = false;
    uint32_t count = audio_catalog_snapshot(tracks, AUDIO_MAX_TRACKS, NULL, &healthy);
    uint32_t total = 0, free = 0;
    if (count > AUDIO_MAX_TRACKS || eva_music_store_space(&total, &free) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "storage info");
    char line[256];
    char revision[40];
    catalog_revision(revision, sizeof(revision));
    int len = snprintf(line, sizeof(line),
                       "{\"count\":%lu,\"total\":%lu,\"free\":%lu,\"healthy\":%s,\"revision\":\"%s\",\"tracks\":[",
                       (unsigned long)count, (unsigned long)total,
                       (unsigned long)free, healthy ? "true" : "false", revision);
    if (len <= 0 || len >= (int)sizeof(line))
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "json");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (httpd_resp_send_chunk(req, line, len) != ESP_OK) return ESP_FAIL;
    for (uint32_t i = 0; i < count; i++) {
        char title[140];
        if (!json_escape(title, sizeof(title), tracks[i].title)) return ESP_FAIL;
        len = snprintf(line, sizeof(line),
                       "%s{\"i\":%lu,\"title\":\"%s\",\"sr\":%u,\"duration\":%lu,\"size\":%lu}",
                       i ? "," : "", (unsigned long)i, title,
                       (unsigned)tracks[i].sample_rate,
                       (unsigned long)tracks[i].duration_ms,
                       (unsigned long)tracks[i].size);
        if (len <= 0 || len >= (int)sizeof(line) ||
            httpd_resp_send_chunk(req, line, len) != ESP_OK) return ESP_FAIL;
    }
    if (httpd_resp_send_chunk(req, "]}", 2) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t upload_post(httpd_req_t *req)
{
    if (s_upload_busy) return send_status(req, "409 Conflict", "upload busy");
    s_upload_busy = true;
    FILE *file = NULL;
    esp_err_t response = ESP_OK;
    uint32_t index = 0;
    uint32_t size = (uint32_t)req->content_len;
    uint32_t free = 0;
    if (req->content_len != size || size == 0 || size > FAM1_MAX_FILE_SIZE) {
        response = send_status(req, "413 Payload Too Large", "invalid audio size");
        goto done;
    }
    if (eva_music_store_space(NULL, &free) != ESP_OK) {
        response = send_status(req, "500 Internal Server Error", "storage info failed");
        goto done;
    }
    if (free < size || free - size < EVA_MUSIC_FREE_RESERVE) {
        response = send_status(req, "507 Insufficient Storage", "not enough storage");
        goto done;
    }
    if (audio_catalog_start_add(size, &file) != ESP_OK) {
        response = send_status(req, "409 Conflict", "catalog unavailable");
        goto done;
    }
    uint8_t chunk[1024];
    uint32_t remaining = size;
    int timeouts = 0;
    while (remaining > 0) {
        int want = remaining < sizeof(chunk) ? (int)remaining : (int)sizeof(chunk);
        int got = httpd_req_recv(req, (char *)chunk, want);
        if (got == HTTPD_SOCK_ERR_TIMEOUT && timeouts++ < 2) continue;
        if (got <= 0 || fwrite(chunk, 1, got, file) != (size_t)got) {
            response = send_status(req, "408 Request Timeout", "upload interrupted");
            goto done;
        }
        timeouts = 0;
        remaining -= (uint32_t)got;
    }
    if (fclose(file) != 0) {
        file = NULL;
        audio_catalog_abort_add(NULL);
        response = send_status(req, "500 Internal Server Error", "write failed");
        goto done;
    }
    file = NULL;
    if (audio_catalog_finish_add(&index) != ESP_OK) {
        response = send_status(req, "422 Unprocessable Content", "audio validation failed");
        goto done;
    }
    ESP_LOGI(TAG, "song uploaded bytes=%lu count=%lu", (unsigned long)size,
             (unsigned long)audio_catalog_count());
    char result[96], revision[40];
    catalog_revision(revision, sizeof(revision));
    snprintf(result, sizeof(result), "{\"index\":%lu,\"revision\":\"%s\"}",
             (unsigned long)index, revision);
    httpd_resp_set_type(req, "application/json");
    response = httpd_resp_sendstr(req, result);
done:
    if (file) audio_catalog_abort_add(file);
    s_upload_busy = false;
    return response;
}

static esp_err_t title_post(httpd_req_t *req)
{
    if (s_upload_busy) return send_status(req, "409 Conflict", "upload busy");
    uint32_t index;
    if (!current_track_query(req, &index))
        return send_status(req, "409 Conflict", "catalog changed; refresh before retrying");
    if (req->content_len < 8 || req->content_len > EVA_TITLE_MAX_BYTES)
        return send_status(req, "413 Payload Too Large", "invalid title size");
    s_upload_busy = true;
    FILE *file = fopen("/fs/title.tmp", "wb");
    esp_err_t response = ESP_OK;
    if (!file) {
        response = send_status(req, "500 Internal Server Error", "title open failed");
        goto done;
    }
    uint8_t chunk[512];
    size_t remaining = req->content_len;
    while (remaining > 0) {
        int want = remaining < sizeof(chunk) ? (int)remaining : (int)sizeof(chunk);
        int got = httpd_req_recv(req, (char *)chunk, want);
        if (got <= 0 || fwrite(chunk, 1, got, file) != (size_t)got) {
            response = send_status(req, "408 Request Timeout", "title upload interrupted");
            goto done;
        }
        remaining -= (size_t)got;
    }
    if (fclose(file) != 0) {
        file = NULL;
        response = send_status(req, "500 Internal Server Error", "title write failed");
        goto done;
    }
    file = NULL;
    if (!eva_title_valid_file("/fs/title.tmp", req->content_len)) {
        response = send_status(req, "422 Unprocessable Content", "invalid title image");
        goto done;
    }
    char target[32];
    if (!eva_title_path(index, target, sizeof(target))) {
        response = send_status(req, "500 Internal Server Error", "title path failed");
        goto done;
    }
    remove(target);
    if (rename("/fs/title.tmp", target) != 0) {
        response = send_status(req, "500 Internal Server Error", "title commit failed");
        goto done;
    }
    response = httpd_resp_sendstr(req, "ok");
done:
    if (file) fclose(file);
    remove("/fs/title.tmp");
    s_upload_busy = false;
    return response;
}

static esp_err_t delete_post(httpd_req_t *req)
{
    if (s_upload_busy) return send_status(req, "409 Conflict", "upload busy");
    uint32_t index;
    if (!current_track_query(req, &index))
        return send_status(req, "409 Conflict", "catalog changed; refresh before retrying");
    s_upload_busy = true;
    esp_err_t result = audio_catalog_delete(index);
    s_upload_busy = false;
    if (result != ESP_OK) return send_status(req, "500 Internal Server Error", "delete failed");
    return httpd_resp_sendstr(req, "ok");
}

esp_err_t eva_wifi_start(void)
{
    if (s_server) return ESP_OK;
    ESP_LOGI(TAG, "heap before Wi-Fi: %lu", (unsigned long)esp_get_free_heap_size());
    if (!s_netif_ready) {
        ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
        esp_err_t err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
        s_netif_ready = true;
    }
    s_ap = esp_netif_create_default_wifi_ap();
    if (!s_ap) return ESP_ERR_NO_MEM;
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&config);
    if (err != ESP_OK) goto fail;
    s_wifi_ready = true;
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK)
        snprintf(s_ssid, sizeof(s_ssid), "EVA-PLAYER-%02X%02X", mac[4], mac[5]);
    wifi_config_t ap = {
        .ap = { .password = "", .authmode = WIFI_AUTH_OPEN,
                .max_connection = 2, .channel = 1, .beacon_interval = 100 },
    };
    strncpy((char *)ap.ap.ssid, s_ssid, sizeof(ap.ap.ssid) - 1);
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_start();
    if (err != ESP_OK) goto fail;
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.max_uri_handlers = 5;
    http.max_open_sockets = 4;
    http.recv_wait_timeout = 2;
    http.send_wait_timeout = 10;
    http.lru_purge_enable = true;
    http.stack_size = 6144;
    snprintf(s_session, sizeof(s_session), "%08lx%08lx",
             (unsigned long)esp_random(), (unsigned long)esp_random());
    err = httpd_start(&s_server, &http);
    if (err != ESP_OK) goto fail;
    const httpd_uri_t routes[] = {
        { .uri = "/", .method = HTTP_GET, .handler = root_get },
        { .uri = "/audio/list", .method = HTTP_GET, .handler = list_get },
        { .uri = "/audio/upload", .method = HTTP_POST, .handler = upload_post },
        { .uri = "/audio/title", .method = HTTP_POST, .handler = title_post },
        { .uri = "/audio/delete", .method = HTTP_POST, .handler = delete_post },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        err = httpd_register_uri_handler(s_server, &routes[i]);
        if (err != ESP_OK) goto fail;
    }
    ESP_LOGI(TAG, "open hotspot ready: %s http://192.168.4.1/ heap=%lu",
             s_ssid, (unsigned long)esp_get_free_heap_size());
    return ESP_OK;
fail:
    eva_wifi_stop();
    return err;
}

esp_err_t eva_wifi_stop(void)
{
    if (s_upload_busy) return ESP_ERR_INVALID_STATE;
    if (s_server) { httpd_stop(s_server); s_server = NULL; }
    if (s_wifi_ready) {
        esp_wifi_stop();
        esp_wifi_deinit();
        s_wifi_ready = false;
    }
    if (s_ap) { esp_netif_destroy_default_wifi(s_ap); s_ap = NULL; }
    return ESP_OK;
}
