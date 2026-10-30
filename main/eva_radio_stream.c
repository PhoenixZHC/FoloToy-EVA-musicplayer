#include "eva_radio_stream.h"
#include <string.h>

void eva_radio_icy_init(eva_radio_icy_t *state, uint32_t interval)
{
    *state = (eva_radio_icy_t){ .interval = interval, .remaining = interval };
}

size_t eva_radio_icy_filter(eva_radio_icy_t *s, uint8_t *data, size_t len)
{
    if (!s->interval) return len;
    size_t out = 0;
    for (size_t i = 0; i < len; i++) {
        if (s->metadata_remaining) {
            s->metadata_remaining--;
        } else if (s->length_pending) {
            s->metadata_remaining = (uint32_t)data[i] * 16U;
            s->length_pending = false;
            s->remaining = s->interval;
        } else {
            data[out++] = data[i];
            if (--s->remaining == 0) s->length_pending = true;
        }
    }
    return out;
}

size_t eva_radio_mp3_frame_size(const uint8_t *d, size_t len)
{
    if (len < 4 || d[0] != 0xff || (d[1] & 0xe0) != 0xe0) return 0;
    unsigned version = (d[1] >> 3) & 3;
    unsigned layer = (d[1] >> 1) & 3;
    unsigned rate = (d[2] >> 2) & 3;
    unsigned br = d[2] >> 4;
    if (version == 1 || layer != 1 || rate == 3 || br == 0 || br == 15 ||
        (d[3] & 3) == 2) return 0;
    static const unsigned rates[] = {44100, 48000, 32000};
    static const unsigned mpeg1[] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
    static const unsigned mpeg2[] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160};
    unsigned hz = rates[rate] / (version == 3 ? 1 : version == 2 ? 2 : 4);
    return (version == 3 ? 144000U * mpeg1[br] : 72000U * mpeg2[br]) / hz +
           ((d[2] >> 1) & 1);
}

bool eva_radio_credentials_valid(const char *ssid, const char *password)
{
    if (!ssid || !password) return false;
    size_t sl = strlen(ssid), pl = strlen(password);
    if (sl == 0 || sl > 32 || pl > 63 || (pl && pl < 8)) return false;
    for (size_t i = 0; i < sl; i++) if ((unsigned char)ssid[i] < 32) return false;
    for (size_t i = 0; i < pl; i++) if ((unsigned char)password[i] < 32) return false;
    return true;
}

uint32_t eva_radio_retry_delay_ms(unsigned attempt)
{
    if (attempt >= 5) return 30000;
    return 1000U << attempt;
}
