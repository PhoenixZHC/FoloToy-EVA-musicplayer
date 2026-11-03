#include "eva_radio_stream.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* 本测试只有一个线程；设备端仍使用标准原子操作。 */
typedef bool atomic_bool;
typedef int atomic_int;
typedef unsigned char atomic_uchar;
typedef unsigned atomic_uint;
#define atomic_load(p) (*(p))
#define atomic_store(p, value) (*(p) = (value))

#define INPUT_SIZE 4096U
#define PCM_SIZE 4608U
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_TIMEOUT 2
#define ESP_ERR_HTTP_EAGAIN 3
#define ESP_AUDIO_ERR_OK 0
#define ESP_AUDIO_ERR_DATA_LACK 1
#define ESP_AUDIO_ERR_MEM_LACK 2
#define EVA_RADIO_STOPPED 0
#define EVA_RADIO_BUFFERING 1
#define EVA_RADIO_PLAYING 2
#define MALLOC_CAP_INTERNAL 0
#define MALLOC_CAP_8BIT 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define pdMS_TO_TICKS(ms) (ms)
typedef int esp_err_t;
typedef int esp_audio_err_t;
typedef unsigned TickType_t;
typedef struct { const char *url; unsigned id; } eva_radio_station_t;
typedef struct { uint8_t *buffer; size_t len, consumed; } esp_audio_dec_in_raw_t;
typedef struct { uint8_t *buffer; size_t len, decoded_size; } esp_audio_dec_out_frame_t;
typedef struct { unsigned bits_per_sample, channel, sample_rate; } esp_audio_dec_info_t;
typedef struct {
    const char *url; int timeout_ms, buffer_size; void *event_handler;
    void *crt_bundle_attach; int max_redirection_count;
} esp_http_client_config_t;
typedef void *esp_http_client_handle_t;

static atomic_bool s_cancel;
static atomic_int s_status;
static atomic_uchar s_volume = 70;
static atomic_uint s_allocation_failures, s_last_failed_size;
static bool s_type_ok, s_headers_valid;
static eva_radio_icy_t s_icy;
static const char *TAG;
static void *http_event, *esp_crt_bundle_attach;
static int64_t now_us, queued_until_us, underrun_us;
static unsigned read_position, writes, max_writes;
static int timeout_ms;
static bool inject_jitter, initial_timeout, short_timeout_seen, fail_read, end_stream;
static bool bad_audio, bad_decode, long_jitter, resumed_after_stall;
static unsigned restart_position, refill_bytes;

static int64_t esp_timer_get_time(void) { return now_us; }
static TickType_t xTaskGetTickCount(void) { return (TickType_t)(now_us / 1000); }
static unsigned esp_get_free_heap_size(void) { return 20000; }
static unsigned heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return 9000; }
static unsigned uxTaskGetStackHighWaterMark(void *task) { (void)task; return 3000; }
static const char *esp_err_to_name(int err) { (void)err; return "mock"; }
static void report_tcp_receive(void *http) { (void)http; }
#define report_stream_diagnostics(...) ((void)0)
static int esp_mp3_dec_open(void *config, int size, void **decoder)
{ (void)config; (void)size; *decoder = (void *)1; return ESP_AUDIO_ERR_OK; }
static void esp_mp3_dec_close(void *decoder) { (void)decoder; }
static int esp_mp3_dec_decode(void *decoder, esp_audio_dec_in_raw_t *raw,
                              esp_audio_dec_out_frame_t *frame, esp_audio_dec_info_t *info)
{
    (void)decoder;
    if (bad_decode && writes) return 99;
    assert(raw->len == 208);
    raw->consumed = raw->len;
    frame->decoded_size = 1152 * 2;
    *info = (esp_audio_dec_info_t){ .bits_per_sample = 16, .channel = 1, .sample_rate = 44100 };
    now_us += 3000;
    return ESP_AUDIO_ERR_OK;
}
static void *esp_http_client_init(const esp_http_client_config_t *config)
{ timeout_ms = config->timeout_ms; return (void *)1; }
static int esp_http_client_set_header(void *h, const char *key, const char *value)
{ (void)h; (void)key; (void)value; return ESP_OK; }
static int esp_http_client_open(void *h, int bytes) { (void)h; (void)bytes; return ESP_OK; }
static int esp_http_client_fetch_headers(void *h) { (void)h; s_type_ok = true; return 0; }
static int esp_http_client_get_status_code(void *h) { (void)h; return 200; }
static int esp_http_client_set_redirection(void *h) { (void)h; return ESP_OK; }
static int esp_http_client_set_timeout_ms(void *h, int value)
{
    (void)h; timeout_ms = value;
    if (value < 10) short_timeout_seen = true;
    return ESP_OK;
}
static void esp_http_client_close(void *h) { (void)h; }
static void esp_http_client_cleanup(void *h) { (void)h; }
static int esp_http_client_get_errno(void *h) { (void)h; return 0; }
static int esp_http_client_read(void *h, char *out, int want)
{
    (void)h;
    assert(want > 0 && want <= (int)INPUT_SIZE);
    if (fail_read && writes) return -1;
    if (end_stream && read_position >= 4160) return 0;
    if (initial_timeout) { initial_timeout = false; now_us += 1500000; return -ESP_ERR_HTTP_EAGAIN; }
    /* 设备已预存音频时，模拟 200 ms 无网络数据；长读取会耗尽 I2S 队列。 */
    int64_t jitter_end_us = long_jitter ? 1800000 : 800000;
    if (inject_jitter && now_us >= 600000 && now_us < jitter_end_us) {
        int64_t wait = jitter_end_us - now_us;
        if (wait > timeout_ms * 1000LL) {
            now_us += timeout_ms * 1000LL;
            return -ESP_ERR_HTTP_EAGAIN;
        }
        now_us += wait;
        restart_position = read_position;
        resumed_after_stall = true;
    }
    static const uint8_t header[] = {0xff, 0xfb, 0x50, 0xc0};
    if (end_stream && (unsigned)want > 4160 - read_position) want = (int)(4160 - read_position);
    for (int i = 0; i < want; i++, read_position++)
        out[i] = read_position % 208 < 4 ? header[read_position % 208] : 0;
    return want;
}
static int bsp_audio_init_playback(void) { return ESP_OK; }
static int bsp_audio_set_format(unsigned hz, unsigned bits, unsigned ch)
{ assert(hz == 44100 && bits == 16 && ch == 1); return ESP_OK; }
static void bsp_audio_set_volume(int volume) { assert(volume == 70); }
static void bsp_audio_deinit(void) {}
static int bsp_audio_write(const void *pcm, size_t bytes)
{
    (void)pcm;
    assert(bytes == 2304);
    if (resumed_after_stall) {
        refill_bytes = read_position - restart_position;
        resumed_after_stall = false;
    }
    if (bad_audio) return ESP_FAIL;
    if (writes && queued_until_us < now_us) underrun_us += now_us - queued_until_us;
    if (queued_until_us < now_us) queued_until_us = now_us;
    queued_until_us += 26122;
    if (queued_until_us - now_us > 32000) now_us = queued_until_us - 32000;
    if (++writes >= max_writes) atomic_store(&s_cancel, true);
    return ESP_OK;
}

#include "radio_player_under_test.inc"

static int run(bool jitter, bool first_timeout)
{
    now_us = queued_until_us = underrun_us = 0;
    writes = read_position = 0; max_writes = 80;
    refill_bytes = restart_position = 0; resumed_after_stall = false;
    inject_jitter = jitter; initial_timeout = first_timeout; short_timeout_seen = false;
    atomic_store(&s_cancel, false); atomic_store(&s_status, EVA_RADIO_STOPPED);
    eva_radio_station_t station = { .url = "http://test/stream", .id = 3 };
    bool played;
    int result = play_stream(&station, &played);
    printf("jitter=%d first_timeout=%d writes=%u underrun_us=%lld short_read=%d result=%d\n",
           jitter, first_timeout, writes, (long long)underrun_us, short_timeout_seen, result);
    if (writes) assert(played);
    return result;
}

int main(void)
{
    setbuf(stdout, NULL);
    assert(run(false, false) == ESP_OK);
    assert(writes == 80);
    assert(run(true, false) == ESP_OK);
    assert(underrun_us < 10000);
    assert(short_timeout_seen);
    long_jitter = true; assert(run(true, false) == ESP_OK);
    printf("refill_after_long_stall=%u\n", refill_bytes);
    assert(refill_bytes >= INPUT_SIZE - 512 - 208); long_jitter = false;
    assert(run(false, true) == ESP_OK);
    fail_read = true; assert(run(false, false) != ESP_OK); fail_read = false;
    bad_audio = true; assert(run(false, false) != ESP_OK); bad_audio = false;
    bad_decode = true; assert(run(false, false) != ESP_OK); bad_decode = false;
    end_stream = true; assert(run(false, false) != ESP_OK); assert(writes == 20); end_stream = false;
    puts("radio player buffering tests passed");
    return 0;
}
