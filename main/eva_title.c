#include "eva_title.h"

#include <stdio.h>
#include <string.h>

static uint8_t s_pixels[EVA_TITLE_MAX_W * EVA_TITLE_H];
static lv_image_dsc_t s_image = {
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_A8,
    .data = s_pixels,
};

bool eva_title_path(uint32_t index, char *out, size_t cap)
{
    int n = snprintf(out, cap, "/fs/t%03lu.bin", (unsigned long)index);
    return n > 0 && (size_t)n < cap;
}

static bool read_header(FILE *file, size_t expected_size, uint16_t *width)
{
    uint8_t header[8];
    if (expected_size < sizeof(header) || fread(header, 1, sizeof(header), file) != sizeof(header))
        return false;
    if (memcmp(header, "ETT1", 4) != 0 || header[6] != EVA_TITLE_H || header[7] != 0)
        return false;
    uint16_t w = (uint16_t)header[4] | ((uint16_t)header[5] << 8);
    if (w == 0 || w > EVA_TITLE_MAX_W || expected_size != 8U + (size_t)w * EVA_TITLE_H)
        return false;
    *width = w;
    return true;
}

bool eva_title_valid_file(const char *path, size_t expected_size)
{
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    uint16_t width = 0;
    bool ok = read_header(file, expected_size, &width);
    if (fclose(file) != 0) ok = false;
    return ok;
}

bool eva_title_load(uint32_t index, const lv_image_dsc_t **image)
{
    char path[32];
    if (!image || !eva_title_path(index, path, sizeof(path))) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return false; }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return false; }
    uint16_t width = 0;
    bool ok = read_header(file, (size_t)size, &width);
    if (ok) ok = fread(s_pixels, 1, (size_t)width * EVA_TITLE_H, file) ==
                 (size_t)width * EVA_TITLE_H;
    if (fclose(file) != 0) ok = false;
    if (!ok) return false;
    s_image.header.w = width;
    s_image.header.h = EVA_TITLE_H;
    s_image.header.stride = width;
    s_image.data_size = (uint32_t)width * EVA_TITLE_H;
    *image = &s_image;
    return true;
}
