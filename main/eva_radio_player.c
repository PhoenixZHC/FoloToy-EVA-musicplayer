#include "eva_radio_player.h"
#include "eva_radio_catalog.h"
#include "eva_radio_config.h"
#include "eva_radio_stream.h"
#include "eva_wifi.h"
#include "bsp_audio.h"
#include "esp_mp3_dec.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/tcpip.h"
#include "lwip/priv/tcp_priv.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define INPUT_SIZE 4096U
#define PCM_SIZE 4608U
static const char *TAG = "eva_radio";
static atomic_bool s_busy, s_cancel;
static atomic_int s_status = EVA_RADIO_STOPPED;
static atomic_uchar s_volume = 70;
static size_t s_station;
static EventGroupHandle_t s_events;
static esp_event_handler_instance_t s_wifi_handler, s_ip_handler;
static eva_radio_icy_t s_icy;
static bool s_type_ok, s_headers_valid;
static atomic_uint s_allocation_failures, s_last_failed_size;
static bool s_heap_callback_registered;

/* 仅在 lwIP 线程读取 PCB，记录接收现场，不更改连接或持有 PCB 指针。
   私有字段依赖当前项目指定的 ESP-IDF 5.5.x，仅用于断流诊断。 */
static void tcp_receive_snapshot(void *arg)
{
    uint16_t port = (uint16_t)(uintptr_t)arg;
    for (const struct tcp_pcb *pcb = tcp_active_pcbs; pcb; pcb = pcb->next) {
        if (pcb->local_port != port) continue;
        unsigned queued = 0, bytes = 0, gap = 0;
#if TCP_QUEUE_OOSEQ
        for (const struct tcp_seg *seg = pcb->ooseq; seg; seg = seg->next) {
            /* tcp_input 已将队列内 seqno 转为主机字节序。 */
            if (!queued) gap = seg->tcphdr->seqno - pcb->rcv_nxt;
            queued++;
            bytes += seg->len;
        }
#endif
        ESP_LOGI(TAG, "TCP receive: state=%u wnd=%u announced=%u ooseq=%u bytes=%u gap=%u refused=%u",
            (unsigned)pcb->state, (unsigned)pcb->rcv_wnd, (unsigned)pcb->rcv_ann_wnd,
            queued, bytes, gap, pcb->refused_data ? (unsigned)pcb->refused_data->tot_len : 0);
        return;
    }
}

static void report_tcp_receive(esp_http_client_handle_t http)
{
    struct sockaddr_storage address;
    socklen_t length = sizeof(address);
    int socket = esp_http_client_get_socket(http);
    if (socket < 0 || getsockname(socket, (struct sockaddr *)&address, &length) != 0) return;
    uint16_t port = address.ss_family == AF_INET ?
        lwip_ntohs(((struct sockaddr_in *)&address)->sin_port) :
        lwip_ntohs(((struct sockaddr_in6 *)&address)->sin6_port);
    err_t err = tcpip_try_callback(tcp_receive_snapshot, (void *)(uintptr_t)port);
    if (err != ERR_OK) ESP_LOGW(TAG, "TCP snapshot unavailable: %d", err);
}

typedef struct {
    int64_t started_us;
    uint32_t received_bytes;
    uint64_t pcm_us;
    uint32_t read_max_us, decode_max_us, write_max_us;
    unsigned consumed_mismatches;
} stream_diagnostics_t;

static void report_stream_diagnostics(const eva_radio_station_t *station,
                                      stream_diagnostics_t *stats, bool force)
{
    int64_t now = esp_timer_get_time();
    if (!force && now - stats->started_us < 30000000) return;
    ESP_LOGI(TAG, "stream stats: station=%u elapsed_ms=%llu bytes=%u pcm_ms=%llu "
        "read_max_us=%u decode_max_us=%u write_max_us=%u consumed_mismatch=%u",
        (unsigned)station->id, (unsigned long long)((now - stats->started_us) / 1000),
        (unsigned)stats->received_bytes, (unsigned long long)(stats->pcm_us / 1000),
        (unsigned)stats->read_max_us, (unsigned)stats->decode_max_us,
        (unsigned)stats->write_max_us, stats->consumed_mismatches);
    *stats = (stream_diagnostics_t){ .started_us = now };
}

static void allocation_failed(size_t size, uint32_t caps, const char *function)
{
    (void)caps;
    (void)function;
    /* 分配失败回调不能再分配内存或阻塞，只记录供正常任务输出。 */
    atomic_store(&s_last_failed_size, (unsigned)size);
    atomic_fetch_add(&s_allocation_failures, 1);
}

static void network_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupClearBits(s_events, BIT1);
        xEventGroupSetBits(s_events, BIT0);
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = data;
        ESP_LOGW(TAG, "Wi-Fi disconnected: reason=%u", event ? event->reason : 0);
        xEventGroupClearBits(s_events, BIT0);
        xEventGroupSetBits(s_events, BIT1);
    }
}

static esp_err_t http_event(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_HEADER) {
        if (!strcasecmp(event->header_key, "icy-metaint")) {
            char *end;
            unsigned long n = strtoul(event->header_value, &end, 10);
            if (*end || n == 0 || n > 1048576) {
                s_headers_valid = false;
                return ESP_FAIL;
            }
            eva_radio_icy_init(&s_icy, (uint32_t)n);
        }
        if (!strcasecmp(event->header_key, "Content-Type"))
            s_type_ok = !strncasecmp(event->header_value, "audio/mpeg", 10) ||
                        !strncasecmp(event->header_value, "audio/mp3", 9);
    }
    return ESP_OK;
}

static esp_err_t play_stream(const eva_radio_station_t *station, bool *played)
{
    *played = false;
    esp_err_t result = ESP_FAIL;
    stream_diagnostics_t stats = { .started_us = esp_timer_get_time() };
    void *decoder = NULL;
    uint8_t *input = malloc(INPUT_SIZE);
    uint8_t *pcm = malloc(PCM_SIZE);
    esp_http_client_handle_t http = NULL;
    ESP_LOGI(TAG, "stream allocation: station=%u heap=%u largest=%u",
        (unsigned)station->id,
        (unsigned)esp_get_free_heap_size(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!input || !pcm) { result = ESP_ERR_NO_MEM; goto done; }
    esp_audio_err_t decoder_err = esp_mp3_dec_open(NULL, 0, &decoder);
    if (decoder_err != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "MP3 decoder open failed: %d", decoder_err);
        if (decoder_err == ESP_AUDIO_ERR_MEM_LACK) result = ESP_ERR_NO_MEM;
        goto done;
    }
    esp_http_client_config_t cfg = {
        .url = station->url, .timeout_ms = 1500, .buffer_size = 1024,
        .event_handler = http_event, .crt_bundle_attach = esp_crt_bundle_attach,
        .max_redirection_count = 3,
    };
    http = esp_http_client_init(&cfg);
    if (!http) { result = ESP_ERR_NO_MEM; goto done; }
    eva_radio_icy_init(&s_icy, 0);
    s_type_ok = false;
    s_headers_valid = true;
    if (esp_http_client_set_header(http, "Icy-MetaData", "0") != ESP_OK) goto done;
    /* 读取模式下显式跟随有限次跳转。 */
    for (int redirect = 0; redirect <= 3 && !atomic_load(&s_cancel); redirect++) {
        result = esp_http_client_open(http, 0);
        if (result != ESP_OK) goto done;
        result = ESP_FAIL;
        if (esp_http_client_fetch_headers(http) < 0) goto done;
        int status = esp_http_client_get_status_code(http);
        ESP_LOGI(TAG, "stream response: station=%u HTTP %d", (unsigned)station->id, status);
        if (status == 200) break;
        if (status < 300 || status >= 400 || redirect == 3 ||
            esp_http_client_set_redirection(http) != ESP_OK) goto done;
        esp_http_client_close(http);
        s_type_ok = false;
        s_headers_valid = true;
        eva_radio_icy_init(&s_icy, 0);
    }
    if (!s_type_ok || !s_headers_valid || atomic_load(&s_cancel)) goto done;
    atomic_store(&s_status, EVA_RADIO_BUFFERING);
    size_t used = 0;
    unsigned invalid = 0;
    bool hardware = false;
    bool prebuffering = true;
    int volume = -1;
    TickType_t last_data = xTaskGetTickCount();
    TickType_t last_report = last_data;
    while (!atomic_load(&s_cancel)) {
        size_t want = INPUT_SIZE - used;
        if (want > 256) want = 256;
        int64_t operation_start = esp_timer_get_time();
        int count = esp_http_client_read(http, (char *)input + used, want);
        uint32_t operation_us = (uint32_t)(esp_timer_get_time() - operation_start);
        if (operation_us > stats.read_max_us) stats.read_max_us = operation_us;
        if (count > 0) stats.received_bytes += (uint32_t)count;
        report_stream_diagnostics(station, &stats, false);
        if (count == -ESP_ERR_HTTP_EAGAIN) {
            atomic_store(&s_status, EVA_RADIO_BUFFERING);
            prebuffering = true;
            report_tcp_receive(http);
            /* 短暂超时不是断流，保留当前半帧；每次读取仍有 1.5 秒取消边界。 */
            if ((TickType_t)(xTaskGetTickCount() - last_data) < pdMS_TO_TICKS(10000)) {
                ESP_LOGW(TAG, "stream read timeout; waiting: heap=%u largest=%u",
                    (unsigned)esp_get_free_heap_size(),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
                ESP_LOGW(TAG, "allocation failures=%u last_size=%u",
                    atomic_load(&s_allocation_failures), atomic_load(&s_last_failed_size));
                continue;
            }
            result = ESP_ERR_TIMEOUT;
        }
        if (count <= 0) {
            ESP_LOGW(TAG, "stream read ended: count=%d errno=%d", count, esp_http_client_get_errno(http));
            goto done;
        }
        last_data = xTaskGetTickCount();
        used += eva_radio_icy_filter(&s_icy, input + used, (size_t)count);
        /* 复用现有 4 KiB 输入区积累约 0.45 秒的 64k 流。
           先储备再输出，让播放期间 TCP 接收队列吸收网络抖动，不新增缓冲分配。 */
        if (prebuffering && used < INPUT_SIZE - 512U) continue;
        prebuffering = false;
        size_t offset = 0;
        while (used - offset >= 4 && !atomic_load(&s_cancel)) {
            size_t frame_size = eva_radio_mp3_frame_size(input + offset, used - offset);
            if (!frame_size) {
                offset++;
                if (++invalid > 65536) goto done;
                continue;
            }
            if (used - offset < frame_size) break;
            esp_audio_dec_in_raw_t raw = { .buffer = input + offset, .len = frame_size };
            esp_audio_dec_out_frame_t frame = { .buffer = pcm, .len = PCM_SIZE };
            esp_audio_dec_info_t info = {0};
            operation_start = esp_timer_get_time();
            esp_audio_err_t err = esp_mp3_dec_decode(decoder, &raw, &frame, &info);
            operation_us = (uint32_t)(esp_timer_get_time() - operation_start);
            if (operation_us > stats.decode_max_us) stats.decode_max_us = operation_us;
            if (raw.consumed != frame_size) stats.consumed_mismatches++;
            offset += frame_size;
            if (err != ESP_AUDIO_ERR_OK && err != ESP_AUDIO_ERR_DATA_LACK) {
                ESP_LOGE(TAG, "MP3 decode failed: err=%d heap=%u largest=%u", err,
                    (unsigned)esp_get_free_heap_size(),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
                if (err == ESP_AUDIO_ERR_MEM_LACK) result = ESP_ERR_NO_MEM;
                goto done;
            }
            if (!frame.decoded_size) continue;
            if (info.bits_per_sample != 16 || info.channel < 1 || info.channel > 2 ||
                info.sample_rate < 8000 || info.sample_rate > 48000 ||
                frame.decoded_size > PCM_SIZE || frame.decoded_size % (2U * info.channel)) {
                ESP_LOGE(TAG, "invalid decoded frame: hz=%u bits=%u channels=%u bytes=%u",
                    (unsigned)info.sample_rate, info.bits_per_sample, info.channel, (unsigned)frame.decoded_size);
                goto done;
            }
            if (!hardware) {
                esp_err_t audio_err = bsp_audio_init_playback();
                if (audio_err != ESP_OK) { result = audio_err; goto done; }
                hardware = true;
            }
            esp_err_t audio_err = bsp_audio_set_format(info.sample_rate, 16, 1);
            if (audio_err != ESP_OK) { result = audio_err; goto done; }
            if (volume != atomic_load(&s_volume)) {
                volume = atomic_load(&s_volume);
                bsp_audio_set_volume(volume);
            }
            size_t samples = frame.decoded_size / 2U;
            int16_t *out = (int16_t *)pcm;
            if (info.channel == 2) {
                samples /= 2U;
                for (size_t i = 0; i < samples; i++)
                    out[i] = ((int32_t)out[2*i] + out[2*i+1]) / 2;
            }
            operation_start = esp_timer_get_time();
            audio_err = bsp_audio_write(pcm, samples * 2U);
            operation_us = (uint32_t)(esp_timer_get_time() - operation_start);
            if (operation_us > stats.write_max_us) stats.write_max_us = operation_us;
            if (audio_err != ESP_OK) {
                ESP_LOGE(TAG, "audio write failed: %s", esp_err_to_name(audio_err));
                result = audio_err;
                goto done;
            }
            stats.pcm_us += (uint64_t)samples * 1000000U / info.sample_rate;
            if (atomic_load(&s_status) != EVA_RADIO_PLAYING ||
                (TickType_t)(xTaskGetTickCount() - last_report) >= pdMS_TO_TICKS(30000)) {
                ESP_LOGI(TAG, "playing: heap=%u largest=%u stack_free=%u",
                    (unsigned)esp_get_free_heap_size(),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                    (unsigned)uxTaskGetStackHighWaterMark(NULL));
                last_report = xTaskGetTickCount();
            }
            invalid = 0;
            *played = true;
            atomic_store(&s_status, EVA_RADIO_PLAYING);
        }
        memmove(input, input + offset, used - offset);
        used -= offset;
        if (used == INPUT_SIZE) goto done;
    }
    result = ESP_OK;
done:
    report_stream_diagnostics(station, &stats, true);
    bsp_audio_deinit();
    if (http) esp_http_client_cleanup(http);
    if (decoder) esp_mp3_dec_close(decoder);
    free(input); free(pcm);
    return result;
}

static void radio_task(void *arg)
{
    (void)arg;
    esp_netif_t *sta = NULL;
    bool wifi_ready = false, wifi_registered = false, ip_registered = false;
    esp_err_t err;
    eva_radio_config_t credentials;
    const eva_radio_station_t *station;
    if (!eva_radio_station_at(s_station, &station) ||
        eva_radio_config_load(&credentials) != ESP_OK) goto done;
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto done;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto done;
    sta = esp_netif_create_default_wifi_sta();
    if (!sta) goto done;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK) goto done;
    wifi_ready = true;
    if (esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
        network_event, NULL, &s_wifi_handler) != ESP_OK) goto done;
    wifi_registered = true;
    if (esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        network_event, NULL, &s_ip_handler) != ESP_OK) goto done;
    ip_registered = true;
    wifi_config_t cfg = {0};
    memcpy(cfg.sta.ssid, credentials.ssid, strlen(credentials.ssid));
    memcpy(cfg.sta.password, credentials.password, strlen(credentials.password));
    memset(&credentials, 0, sizeof(credentials));
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_STA);
    cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    cfg.sta.failure_retry_cnt = 3;
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    memset(&cfg, 0, sizeof(cfg));
    if (err != ESP_OK || esp_wifi_start() != ESP_OK) goto done;
    /* 连续音频需要及时收包，关闭默认的 modem sleep；不影响上传 AP。 */
    err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "disable Wi-Fi power save failed: %s", esp_err_to_name(err));
        goto done;
    }
    unsigned attempt = 0;
    bool reconnecting = false;
    while (!atomic_load(&s_cancel)) {
        if (!(xEventGroupGetBits(s_events) & BIT0)) {
            atomic_store(&s_status, reconnecting ? EVA_RADIO_RECONNECTING : EVA_RADIO_CONNECTING);
            xEventGroupClearBits(s_events, BIT0 | BIT1);
            err = esp_wifi_connect();
            if (err != ESP_OK) ESP_LOGW(TAG, "Wi-Fi connect failed: %s", esp_err_to_name(err));
            if (err == ESP_OK) {
                for (int tick = 0; tick < 150 && !atomic_load(&s_cancel); tick++) {
                    EventBits_t bits = xEventGroupWaitBits(s_events, BIT0 | BIT1,
                        pdFALSE, pdFALSE, pdMS_TO_TICKS(100));
                    if (bits & (BIT0 | BIT1)) break;
                }
            }
            if (!(xEventGroupGetBits(s_events) & BIT0)) {
                /* 超时后终止仍在进行的连接，下一轮不能叠加扫描。 */
                esp_wifi_disconnect();
                xEventGroupWaitBits(s_events, BIT1, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));
            }
        }
        bool played = false;
        if (!atomic_load(&s_cancel) && (xEventGroupGetBits(s_events) & BIT0)) {
            esp_err_t stream_err = play_stream(station, &played);
            if (!atomic_load(&s_cancel))
                ESP_LOGW(TAG, "stream ended: %s", esp_err_to_name(stream_err));
            /* 内存不足应明确失败，不能持续分配并隐藏故障。 */
            if (stream_err == ESP_ERR_NO_MEM) goto done;
        }
        if (atomic_load(&s_cancel)) break;
        if (played) attempt = 0;
        uint32_t delay_ms = eva_radio_retry_delay_ms(attempt);
        if (attempt < 5) attempt++;
        reconnecting = true;
        atomic_store(&s_status, EVA_RADIO_RECONNECTING);
        ESP_LOGW(TAG, "reconnecting in %u ms", (unsigned)delay_ms);
        /* 电台断流保留已连接的 STA；等待每 100ms 检查一次取消。 */
        for (uint32_t elapsed = 0; elapsed < delay_ms && !atomic_load(&s_cancel); elapsed += 100)
            vTaskDelay(pdMS_TO_TICKS(100));
    }
done:
    memset(&credentials, 0, sizeof(credentials));
    if (wifi_registered) esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, s_wifi_handler);
    if (ip_registered) esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_handler);
    if (wifi_ready) { esp_wifi_stop(); esp_wifi_deinit(); }
    if (sta) esp_netif_destroy_default_wifi(sta);
    ESP_LOGI(TAG, "released: heap=%u largest=%u stack_free=%u",
        (unsigned)esp_get_free_heap_size(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)uxTaskGetStackHighWaterMark(NULL));
    atomic_store(&s_status, atomic_load(&s_cancel) ? EVA_RADIO_STOPPED : EVA_RADIO_FAILED);
    atomic_store(&s_busy, false);
    vTaskDelete(NULL);
}

esp_err_t eva_radio_player_start(size_t station, uint8_t volume)
{
    const eva_radio_station_t *entry;
    if (eva_wifi_running() || atomic_load(&s_busy)) return ESP_ERR_INVALID_STATE;
    if (!s_heap_callback_registered) {
        esp_err_t hook_err = heap_caps_register_failed_alloc_callback(allocation_failed);
        if (hook_err != ESP_OK) return hook_err;
        s_heap_callback_registered = true;
    }
    if (!eva_radio_station_at(station, &entry) || volume > 100) return ESP_ERR_INVALID_ARG;
    eva_radio_config_t config;
    esp_err_t err = eva_radio_config_load(&config);
    memset(&config, 0, sizeof(config));
    if (err != ESP_OK) {
        atomic_store(&s_status, EVA_RADIO_FAILED);
        return err;
    }
    if (!s_events) s_events = xEventGroupCreate();
    if (!s_events) return ESP_ERR_NO_MEM;
    xEventGroupClearBits(s_events, BIT0 | BIT1);
    s_station = station;
    atomic_store(&s_cancel, false);
    atomic_store(&s_volume, volume);
    atomic_store(&s_status, EVA_RADIO_CONNECTING);
    atomic_store(&s_busy, true);
    if (xTaskCreate(radio_task, "eva_radio", 6144, NULL, 4, NULL) != pdPASS) {
        atomic_store(&s_busy, false); atomic_store(&s_status, EVA_RADIO_FAILED);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
bool eva_radio_player_stop(void)
{
    atomic_store(&s_cancel, true);
    return !atomic_load(&s_busy);
}
bool eva_radio_player_busy(void) { return atomic_load(&s_busy); }
eva_radio_status_t eva_radio_player_status(void) { return atomic_load(&s_status); }
void eva_radio_player_volume(uint8_t volume) { if (volume <= 100) atomic_store(&s_volume, volume); }
