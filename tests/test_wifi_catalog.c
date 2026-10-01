/* Real HTTP handlers and catalog, with only ESP/network APIs stubbed on host. */
#include <assert.h>
#include <stdlib.h>
#include "../main/audio_catalog.c"
#include "../main/eva_wifi.c"

esp_err_t eva_music_store_space(uint32_t *total, uint32_t *free_bytes)
{
    if (total) *total = 0x5f0000;
    if (free_bytes) *free_bytes = 0x400000;
    return ESP_OK;
}

static void request_for(httpd_req_t *req, unsigned index, const char *revision)
{
    memset(req, 0, sizeof(*req));
    snprintf(req->query, sizeof(req->query), "i=%u&r=%s", index, revision);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(audio_catalog_scan_path(&s_catalog, argv[1]));
    assert(audio_catalog_count() == 3);
    assert(eva_wifi_start() == ESP_OK);
    char old_revision[40], new_revision[40];
    catalog_revision(old_revision, sizeof(old_revision));
    httpd_req_t req = { 0 };
    assert(list_get(&req) == ESP_OK);
    assert(strstr(req.response, old_revision));

    /* Retain one valid upload body before deleting its original song. */
    char path[AUDIO_PATH_MAX];
    assert(audio_catalog_name_path(&s_catalog, 0, path, sizeof(path)));
    uint32_t size = s_catalog.items[0].size;
    unsigned char *audio = malloc(size);
    FILE *file = fopen(path, "rb");
    assert(audio && file && fread(audio, 1, size, file) == size);
    assert(fclose(file) == 0);

    request_for(&req, 0, old_revision);
    assert(delete_post(&req) == ESP_OK);
    assert(strcmp(req.response, "ok") == 0 && audio_catalog_count() == 2);
    request_for(&req, 1, old_revision);
    assert(delete_post(&req) == ESP_OK);
    assert(strncmp(req.status, "409", 3) == 0 && audio_catalog_count() == 2);
    assert(strcmp(s_catalog.items[1].title, "One Last Kiss") == 0);
    request_for(&req, 1, old_revision);
    assert(title_post(&req) == ESP_OK && strncmp(req.status, "409", 3) == 0);
    assert(eva_title_path(1, path, sizeof(path)));
    assert(strcmp(path, "/fs/t002.bin") == 0);

    /* Missing revision from an old cached page must not delete anything. */
    request_for(&req, 0, "");
    strcpy(req.query, "i=0");
    assert(delete_post(&req) == ESP_OK && strncmp(req.status, "409", 3) == 0);
    assert(audio_catalog_count() == 2);

    catalog_revision(old_revision, sizeof(old_revision));
    memset(&req, 0, sizeof(req));
    req.content_len = size;
    req.body = audio;
    assert(upload_post(&req) == ESP_OK);
    assert(strstr(req.response, "\"index\":0,"));
    catalog_revision(new_revision, sizeof(new_revision));
    assert(strcmp(old_revision, new_revision) != 0);
    assert(strstr(req.response, new_revision) && audio_catalog_count() == 3);
    request_for(&req, 0, old_revision);
    assert(delete_post(&req) == ESP_OK && strncmp(req.status, "409", 3) == 0);

    /* Same generation after reopening the AP still invalidates old pages. */
    strcpy(old_revision, new_revision);
    assert(eva_wifi_stop() == ESP_OK && eva_wifi_start() == ESP_OK);
    request_for(&req, 0, old_revision);
    assert(delete_post(&req) == ESP_OK && strncmp(req.status, "409", 3) == 0);
    catalog_revision(new_revision, sizeof(new_revision));
    request_for(&req, 0, new_revision);
    assert(title_post(&req) == ESP_OK && strncmp(req.status, "413", 3) == 0);
    request_for(&req, 0, new_revision);
    assert(delete_post(&req) == ESP_OK && strcmp(req.response, "ok") == 0);
    assert(audio_catalog_count() == 2);
    free(audio);
    puts("HTTP catalog: stale delete/title, missing revision, upload slot/revision, AP restart and current request passed");
    return 0;
}
