#include "eva_radio_catalog.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(eva_radio_city_count() >= 19 && eva_radio_city_count() <= 32);
    assert(eva_radio_station_count() >= 89);
    assert(eva_radio_station_at(eva_radio_station_count(), NULL) == false);

    const eva_radio_city_t *city = NULL;
    assert(eva_radio_city_at(0, &city));
    assert(city != NULL && city->name != NULL);

    const eva_radio_station_t *station = NULL;
    assert(eva_radio_station_at(0, &station));
    assert(station != NULL && station->name != NULL);
    assert(station->city_id == city->id);
    assert(station->codec == EVA_RADIO_CODEC_MP3);
    assert(station->bitrate_kbps >= 32 && station->bitrate_kbps <= 128);
    assert(station->url[0] != '\0');

    for (size_t i = 0; i < eva_radio_city_count(); i++) {
        assert(eva_radio_city_at(i, &city));
        size_t count = eva_radio_stations_for_city(city->id);
        assert(count > 0 && count <= 32);
        assert(city->display_name && city->display_name[0]);
        for (size_t j = 0; j < count; j++) {
            assert(eva_radio_station_for_city_at(city->id, j, &station));
            assert(station->city_id == city->id);
            assert(station->codec == EVA_RADIO_CODEC_MP3);
            assert(station->bitrate_kbps >= 32 && station->bitrate_kbps <= 128);
            assert(station->url[0] != '\0');
            assert(station->source_url[0] != '\0');
            assert(station->verified_on[0] != '\0');
            assert(station->display_name && station->display_name[0]);
            assert(strncmp(station->url, "http://", 7) == 0 ||
                   strncmp(station->url, "https://", 8) == 0);
            assert(strstr(station->url, ".m3u8") == NULL);
            for (size_t k = 0; k < eva_radio_station_count(); k++) {
                const eva_radio_station_t *other;
                assert(eva_radio_station_at(k, &other));
                if (other != station) {
                    assert(other->id != station->id);
                    assert(strcmp(other->url, station->url) != 0);
                }
            }
        }
        assert(!eva_radio_station_for_city_at(city->id, count, &station));
    }
    assert(eva_radio_stations_for_city(UINT16_MAX) == 0);
    assert(!eva_radio_city_at(eva_radio_city_count(), &city));
    assert(!eva_radio_station_for_city_at(UINT16_MAX, 0, &station));

    /* 已保存的最近电台以 ID 持久化；扩台不能改变旧 ID 的音源。 */
    const char *legacy_ids[] = {"/275/", "/267/", "/20194/", "/1270/",
        "/15318146/", "/1297/", "/15318703/", "/20198/", "/2803/",
        "/1673/", "/1670/", "/20212380/", "/1737/"};
    for (size_t i = 0; i < sizeof(legacy_ids) / sizeof(legacy_ids[0]); i++) {
        assert(eva_radio_station_at(i, &station));
        assert(station->id == i + 2);
        assert(strstr(station->url, legacy_ids[i]));
    }
    puts("radio catalog tests passed");
    return 0;
}
