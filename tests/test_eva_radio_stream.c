#include "eva_radio_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    eva_radio_icy_t state;
    eva_radio_icy_init(&state, 4);
    uint8_t first[] = {1, 2, 3, 4, 1, 9, 8};
    uint8_t second[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,11,12};
    size_t a = eva_radio_icy_filter(&state, first, sizeof(first));
    size_t b = eva_radio_icy_filter(&state, second, sizeof(second));
    assert(a == 4 && memcmp(first, (uint8_t[]){1,2,3,4}, 4) == 0);
    assert(b == 4 && memcmp(second, (uint8_t[]){5,0,11,12}, 4) == 0);
    uint8_t empty_metadata[] = {0,21,22,23,24,0,31};
    assert(eva_radio_icy_filter(&state, empty_metadata, sizeof(empty_metadata)) == 5);
    assert(memcmp(empty_metadata, (uint8_t[]){21,22,23,24,31}, 5) == 0);
    eva_radio_icy_init(&state, 0);
    assert(eva_radio_icy_filter(&state, empty_metadata, 7) == 7);
    assert(eva_radio_mp3_frame_size((uint8_t[]){0xff,0xfb,0x90,0x64}, 4) == 417);
    assert(eva_radio_mp3_frame_size((uint8_t[]){0xff,0xfb,0x92,0x64}, 4) == 418);
    assert(eva_radio_mp3_frame_size((uint8_t[]){0xff,0xf3,0x80,0x00}, 4) == 208);
    assert(eva_radio_mp3_frame_size((uint8_t[]){0xff,0xe3,0x80,0x00}, 4) == 417);
    assert(eva_radio_mp3_frame_size((uint8_t[]){0xff,0xfb,0x90}, 3) == 0);
    assert(eva_radio_mp3_frame_size((uint8_t[]){0xff,0xeb,0x90,0x00}, 4) == 0);
    assert(eva_radio_mp3_frame_size((uint8_t[]){0xff,0xfb,0xfc,0x64}, 4) == 0);
    assert(eva_radio_credentials_valid("router", "12345678"));
    assert(eva_radio_credentials_valid("open", ""));
    assert(!eva_radio_credentials_valid("", "12345678"));
    assert(!eva_radio_credentials_valid("router", "short"));
    char ssid[34], password[65];
    memset(ssid, 'a', 32); ssid[32] = 0;
    memset(password, 'b', 63); password[63] = 0;
    assert(eva_radio_credentials_valid(ssid, password));
    ssid[32] = 'a'; ssid[33] = 0;
    assert(!eva_radio_credentials_valid(ssid, password));
    ssid[32] = 0; password[63] = 'b'; password[64] = 0;
    assert(!eva_radio_credentials_valid(ssid, password));
    assert(!eva_radio_credentials_valid(NULL, ""));
    assert(!eva_radio_credentials_valid("line\nbreak", ""));
    assert(eva_radio_retry_delay_ms(0) == 1000);
    assert(eva_radio_retry_delay_ms(1) == 2000);
    assert(eva_radio_retry_delay_ms(4) == 16000);
    assert(eva_radio_retry_delay_ms(5) == 30000);
    assert(eva_radio_retry_delay_ms(200) == 30000);
    puts("radio stream tests passed");
    return 0;
}
