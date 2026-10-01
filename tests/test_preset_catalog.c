#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "audio_catalog.h"

/* Run only against a disposable COPY of the generated preset directory. */
int main(int argc, char **argv)
{
    assert(argc == 2);
    audio_catalog_t catalog, rebooted;
    assert(audio_catalog_scan_path(&catalog, argv[1]));
    assert(catalog.healthy && catalog.count == 3);
    audio_track_t first = catalog.items[0];
    audio_track_t last = catalog.items[2];

    /* Delete a middle preset; other song/title slots must remain unchanged. */
    char path[AUDIO_PATH_MAX];
    snprintf(path, sizeof(path), "%s/t002.bin", argv[1]);
    FILE *title = fopen(path, "rb");
    assert(title);
    unsigned char expected[8 + 768 * 34];
    size_t size = fread(expected, 1, sizeof(expected), title);
    assert(size > 8 && !ferror(title));
    assert(fclose(title) == 0);
    assert(audio_catalog_delete_path(&catalog, 1));
    assert(catalog.count == 2);
    assert(strcmp(catalog.items[0].title, first.title) == 0);
    assert(strcmp(catalog.items[1].title, last.title) == 0);
    snprintf(path, sizeof(path), "%s/t002.bin", argv[1]);
    title = fopen(path, "rb");
    assert(title);
    unsigned char actual[sizeof(expected)];
    assert(fread(actual, 1, sizeof(actual), title) == size);
    assert(memcmp(actual, expected, size) == 0);
    assert(fclose(title) == 0);

    /* Boot-time rescan must not resurrect the removed preset. */
    assert(audio_catalog_scan_path(&rebooted, argv[1]));
    assert(rebooted.count == 2 && rebooted.healthy);
    assert(strcmp(rebooted.items[1].title, last.title) == 0);

    /* Append through the same commit path used by a browser upload. */
    char source[AUDIO_PATH_MAX], temp[AUDIO_PATH_MAX];
    assert(audio_catalog_name_path(&rebooted, 0, source, sizeof(source)));
    assert(audio_catalog_temp_name_path(&rebooted, temp, sizeof(temp)));
    FILE *in = fopen(source, "rb"), *out = fopen(temp, "wb");
    assert(in && out);
    unsigned char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), in)) > 0)
        assert(fwrite(chunk, 1, n, out) == n);
    assert(!ferror(in));
    assert(fclose(in) == 0 && fclose(out) == 0);
    assert(audio_catalog_commit_temp_path(&rebooted, first.size, NULL));
    assert(rebooted.count == 3);
    assert(audio_catalog_scan_path(&catalog, argv[1]));
    assert(catalog.count == 3);

    while (catalog.count) assert(audio_catalog_delete_path(&catalog, 0));
    assert(audio_catalog_scan_path(&rebooted, argv[1]));
    assert(rebooted.healthy && rebooted.count == 0);
    puts("preset catalog: 6 scenarios passed (scan, delete, title pairing, rescan, append, empty)");
    return 0;
}
