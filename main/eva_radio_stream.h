#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t interval;
    uint32_t remaining;
    uint32_t metadata_remaining;
    bool length_pending;
} eva_radio_icy_t;

void eva_radio_icy_init(eva_radio_icy_t *state, uint32_t interval);
/* 原地移除跨 HTTP 分块的 ICY 元数据，不额外分配内存。 */
size_t eva_radio_icy_filter(eva_radio_icy_t *state, uint8_t *data, size_t len);
/* 返回完整 MP3 帧长；无效头返回 0，调用方保留不足四字节的尾部。 */
size_t eva_radio_mp3_frame_size(const uint8_t *data, size_t len);
bool eva_radio_credentials_valid(const char *ssid, const char *password);
uint32_t eva_radio_retry_delay_ms(unsigned attempt);
