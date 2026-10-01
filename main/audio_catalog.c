#include "audio_catalog.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "fam1_format.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

typedef struct {
    FILE *file;
    uint32_t size;
    uint32_t position;
#ifdef ESP_PLATFORM
    uint32_t bytes_since_yield;
#endif
} file_reader_t;

static audio_catalog_t s_catalog;
static bool s_upload_active;
static uint32_t s_upload_expected_size;

static bool catalog_fail(audio_catalog_t *catalog, audio_catalog_error_t error,
                         uint32_t index)
{
    catalog->healthy = false;
    catalog->error = error;
    catalog->error_index = index;
    return false;
}

static bool make_path(char *out, size_t out_size, const char *root,
                      uint32_t index)
{
    int written = snprintf(out, out_size, "%s/a%03lu.fam", root,
                           (unsigned long)index);
    return written > 0 && (size_t)written < out_size;
}

static bool make_title_path(char *out, size_t out_size, const char *root,
                            uint32_t index)
{
    int written = snprintf(out, out_size, "%s/t%03lu.bin", root,
                           (unsigned long)index);
    return written > 0 && (size_t)written < out_size;
}

static bool get_file_size(const char *path, uint32_t *size)
{
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size < 0 || (uint64_t)st.st_size > UINT32_MAX)
        return false;
    if (size) *size = (uint32_t)st.st_size;
    return true;
}

static bool file_reader_read(void *ctx, uint32_t offset, void *out, size_t len)
{
    file_reader_t *reader = ctx;
    if (!reader || !out || (uint64_t)offset + len > reader->size) return false;
    if (offset != reader->position &&
        fseek(reader->file, (long)offset, SEEK_SET) != 0) return false;
    if (fread(out, 1, len, reader->file) != len) return false;
    reader->position = offset + (uint32_t)len;
#ifdef ESP_PLATFORM
    reader->bytes_since_yield += (uint32_t)len;
    if (reader->bytes_since_yield >= 16384u) {
        reader->bytes_since_yield = 0;
        vTaskDelay(1);
    }
#endif
    return true;
}

static bool inspect_track(const char *path, audio_track_t *track)
{
    uint32_t size = 0;
    if (!get_file_size(path, &size)) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    file_reader_t file_reader = { .file = file, .size = size };
    fam1_reader_t reader = { .read = file_reader_read, .ctx = &file_reader };
    fam1_header_t header;
    bool ok = fam1_validate(&reader, size, &header);
    if (fclose(file) != 0) ok = false;
    if (!ok) return false;

    memset(track, 0, sizeof(*track));
    track->size = size;
    track->sample_rate = header.sample_rate;
    track->sample_count = header.sample_count;
    track->duration_ms = header.duration_ms;
    memcpy(track->title, header.title, sizeof(track->title));
    track->title[sizeof(track->title) - 1u] = '\0';
    return true;
}

bool audio_catalog_name_path(const audio_catalog_t *catalog, uint32_t index,
                             char *out, size_t out_size)
{
    if (!catalog || !out || index >= catalog->count) return false;
    return make_path(out, out_size, catalog->root, catalog->items[index].file_id);
}

bool audio_catalog_temp_name_path(const audio_catalog_t *catalog,
                                  char *out, size_t out_size)
{
    if (!catalog || !out) return false;
    int written = snprintf(out, out_size, "%s/%s", catalog->root, AUDIO_TEMP_NAME);
    return written > 0 && (size_t)written < out_size;
}

bool audio_catalog_scan_path(audio_catalog_t *catalog, const char *root)
{
    if (!catalog || !root || !root[0] || strlen(root) >= sizeof(catalog->root))
        return false;
    memset(catalog, 0, sizeof(*catalog));
    memcpy(catalog->root, root, strlen(root) + 1u);
    catalog->healthy = true;
    catalog->layout_generation = 1;

    for (uint32_t index = 0; index < AUDIO_MAX_TRACKS; index++) {
        char path[AUDIO_PATH_MAX];
        struct stat st;
        if (!make_path(path, sizeof(path), root, index))
            return catalog_fail(catalog, AUDIO_CATALOG_PATH, index);
        if (stat(path, &st) != 0) {
            if (errno == ENOENT) continue;
            return catalog_fail(catalog, AUDIO_CATALOG_IO, index);
        }
        audio_track_t *track = &catalog->items[catalog->count];
        if (!inspect_track(path, track))
            return catalog_fail(catalog, AUDIO_CATALOG_INVALID_FILE, index);
        track->file_id = index;
        catalog->count++;
    }
    return true;
}

bool audio_catalog_commit_temp_path(audio_catalog_t *catalog, uint32_t expected_size,
                                    audio_track_t *committed)
{
    if (!catalog || !catalog->healthy || catalog->count >= AUDIO_MAX_TRACKS)
        return false;
    char temp[AUDIO_PATH_MAX], final[AUDIO_PATH_MAX];
    audio_track_t inspected;
    uint32_t actual_size = 0;
    if (!audio_catalog_temp_name_path(catalog, temp, sizeof(temp)) ||
        !get_file_size(temp, &actual_size) ||
        actual_size != expected_size ||
        !inspect_track(temp, &inspected)) {
        return false;
    }
    uint32_t slot;
    for (slot = 0; slot < AUDIO_MAX_TRACKS; slot++) {
        struct stat st;
        if (!make_path(final, sizeof(final), catalog->root, slot)) return false;
        if (stat(final, &st) == 0) continue;
        if (errno != ENOENT) return false;
        break;
    }
    if (slot == AUDIO_MAX_TRACKS) return false;
    /* A prior interrupted operation must not give the new song an old title. */
    char title[AUDIO_PATH_MAX];
    if (!make_title_path(title, sizeof(title), catalog->root, slot) ||
        (remove(title) != 0 && errno != ENOENT)) return false;
    if (rename(temp, final) != 0) return false;
    inspected.file_id = slot;
    uint32_t index = 0;
    while (index < catalog->count && catalog->items[index].file_id < slot) index++;
    memmove(&catalog->items[index + 1u], &catalog->items[index],
            (catalog->count - index) * sizeof(catalog->items[0]));
    catalog->items[index] = inspected;
    catalog->count++;
    catalog->layout_generation++;
    if (committed) *committed = inspected;
    return true;
}

bool audio_catalog_delete_path(audio_catalog_t *catalog, uint32_t index)
{
    if (!catalog || !catalog->healthy || index >= catalog->count) return false;
    char path[AUDIO_PATH_MAX];
    char title[AUDIO_PATH_MAX];
    uint32_t slot = catalog->items[index].file_id;
    if (!audio_catalog_name_path(catalog, index, path, sizeof(path)) ||
        !make_title_path(title, sizeof(title), catalog->root, slot)) return false;
    /* Keep other songs in their original slots. If interrupted here, this
       song either remains (possibly without its optional mask) or is absent.
       Both states can be scanned on boot without a journal or renumbering. */
    if (remove(title) != 0 && errno != ENOENT) return false;
    if (remove(path) != 0) return false;
    catalog->layout_generation++;
    memmove(&catalog->items[index], &catalog->items[index + 1u],
            (catalog->count - index - 1u) * sizeof(catalog->items[0]));
    catalog->count--;
    memset(&catalog->items[catalog->count], 0, sizeof(catalog->items[0]));
    return true;
}

esp_err_t audio_catalog_init(void)
{
    return audio_catalog_scan_path(&s_catalog, AUDIO_ROOT_PATH) ? ESP_OK : ESP_FAIL;
}

uint32_t audio_catalog_count(void)
{
    return s_catalog.count;
}

bool audio_catalog_healthy(void)
{
    return s_catalog.healthy;
}

uint32_t audio_catalog_generation(void)
{
    return s_catalog.layout_generation;
}

uint32_t audio_catalog_snapshot(audio_track_t *items, uint32_t capacity,
                                uint32_t *generation, bool *healthy)
{
    uint32_t copied = s_catalog.count < capacity ? s_catalog.count : capacity;
    if (items && copied > 0)
        memcpy(items, s_catalog.items, (size_t)copied * sizeof(items[0]));
    if (generation) *generation = s_catalog.layout_generation;
    if (healthy) *healthy = s_catalog.healthy;
    return s_catalog.count;
}

bool audio_catalog_get(uint32_t index, audio_track_t *out)
{
    if (!out || !s_catalog.healthy || index >= s_catalog.count) return false;
    *out = s_catalog.items[index];
    return true;
}

esp_err_t audio_catalog_start_add(uint32_t expected_size, FILE **file)
{
    if (!file) return ESP_ERR_INVALID_ARG;
    *file = NULL;
    if (!s_catalog.healthy || s_upload_active) return ESP_ERR_INVALID_STATE;
    if (s_catalog.count >= AUDIO_MAX_TRACKS) return ESP_ERR_NO_MEM;
    if (expected_size == 0 || expected_size > FAM1_MAX_FILE_SIZE) return ESP_ERR_INVALID_SIZE;

    char temp[AUDIO_PATH_MAX];
    if (!audio_catalog_temp_name_path(&s_catalog, temp, sizeof(temp))) return ESP_FAIL;
    remove(temp);
    FILE *opened = fopen(temp, "wb");
    if (!opened) return ESP_FAIL;
    s_upload_active = true;
    s_upload_expected_size = expected_size;
    *file = opened;
    return ESP_OK;
}

esp_err_t audio_catalog_finish_add(uint32_t *index)
{
    if (!s_upload_active) return ESP_ERR_INVALID_STATE;
    audio_track_t committed;
    bool ok = audio_catalog_commit_temp_path(&s_catalog, s_upload_expected_size, &committed);
    if (ok && index) {
        for (uint32_t i = 0; i < s_catalog.count; i++) {
            if (s_catalog.items[i].file_id == committed.file_id) {
                *index = i;
                break;
            }
        }
    }
    if (!ok) {
        char temp[AUDIO_PATH_MAX];
        if (audio_catalog_temp_name_path(&s_catalog, temp, sizeof(temp)))
            remove(temp);
    }
    s_upload_active = false;
    s_upload_expected_size = 0;
    return ok ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t audio_catalog_abort_add(FILE *file)
{
    bool ok = true;
    if (file && fclose(file) != 0) ok = false;
    char temp[AUDIO_PATH_MAX];
    if (!audio_catalog_temp_name_path(&s_catalog, temp, sizeof(temp))) ok = false;
    else if (remove(temp) != 0 && errno != ENOENT) ok = false;
    s_upload_active = false;
    s_upload_expected_size = 0;
    return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t audio_catalog_delete(uint32_t index)
{
    return audio_catalog_delete_path(&s_catalog, index) ? ESP_OK : ESP_FAIL;
}

bool audio_catalog_read_at(uint32_t index, uint32_t expected_generation,
                           uint32_t offset, void *out, size_t len)
{
    if (!s_catalog.healthy || !out || index >= s_catalog.count ||
        expected_generation != s_catalog.layout_generation ||
        (uint64_t)offset + len > s_catalog.items[index].size) {
        return false;
    }
    char path[AUDIO_PATH_MAX];
    if (!audio_catalog_name_path(&s_catalog, index, path, sizeof(path))) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    bool ok = fseek(file, (long)offset, SEEK_SET) == 0 &&
              fread(out, 1, len, file) == len;
    if (fclose(file) != 0) ok = false;
    return ok;
}
