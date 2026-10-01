/* Each run receives a disposable copy of the three factory songs. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *fail_remove;
int catalog_test_remove(const char *path);
#define remove catalog_test_remove
#include "../main/audio_catalog.c"
#undef remove

int catalog_test_remove(const char *path)
{
    if (fail_remove && strstr(path, fail_remove)) {
        errno = EIO;
        return -1;
    }
    return remove(path);
}

static void copy_file(const char *source, const char *target)
{
    FILE *in = fopen(source, "rb"), *out = fopen(target, "wb");
    assert(in && out);
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) != 0)
        assert(fwrite(buf, 1, n, out) == n);
    assert(!ferror(in));
    assert(fclose(in) == 0 && fclose(out) == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    audio_catalog_t catalog, reboot;
    assert(audio_catalog_scan_path(&catalog, argv[2]));
    if (strcmp(argv[1], "gap") == 0) {
        assert(catalog.healthy && catalog.count == 2);
        assert(strcmp(catalog.items[1].title, "One Last Kiss") == 0);
    } else if (strcmp(argv[1], "failure") == 0) {
        assert(catalog.count == 3);
        fail_remove = "t001.bin";
        assert(!audio_catalog_delete_path(&catalog, 1));
        assert(catalog.healthy && catalog.count == 3);
        assert(audio_catalog_scan_path(&reboot, argv[2]) && reboot.count == 3);
        fail_remove = "a001.fam";
        assert(!audio_catalog_delete_path(&catalog, 1));
        assert(catalog.healthy && catalog.count == 3);
        assert(audio_catalog_scan_path(&reboot, argv[2]) && reboot.count == 3);
        fail_remove = NULL;
        assert(audio_catalog_delete_path(&reboot, 1));
        assert(audio_catalog_scan_path(&catalog, argv[2]) && catalog.count == 2);
    } else {
        assert(catalog.count == 3);
        uint32_t revision = catalog.layout_generation;
        assert(audio_catalog_delete_path(&catalog, 1));
        assert(catalog.healthy && catalog.count == 2);
        assert(catalog.layout_generation != revision);
        char path[AUDIO_PATH_MAX], temp[AUDIO_PATH_MAX];
        assert(audio_catalog_name_path(&catalog, 1, path, sizeof(path)));
        assert(strstr(path, "a002.fam"));
        snprintf(path, sizeof(path), "%s/t002.bin", argv[2]);
        struct stat st;
        assert(stat(path, &st) == 0);
        assert(audio_catalog_scan_path(&reboot, argv[2]) && reboot.count == 2);
        assert(strcmp(reboot.items[1].title, "One Last Kiss") == 0);
        revision = catalog.layout_generation;
        assert(audio_catalog_name_path(&catalog, 0, path, sizeof(path)));
        assert(audio_catalog_temp_name_path(&catalog, temp, sizeof(temp)));
        copy_file(path, temp);
        assert(audio_catalog_commit_temp_path(&catalog, catalog.items[0].size, NULL));
        assert(catalog.count == 3 && catalog.layout_generation != revision);
        assert(audio_catalog_name_path(&catalog, 1, path, sizeof(path)));
        assert(strstr(path, "a001.fam"));
        assert(strcmp(catalog.items[2].title, "One Last Kiss") == 0);
        assert(audio_catalog_scan_path(&reboot, argv[2]) && reboot.count == 3);
        for (uint32_t i = 0; i < 3; i++)
            assert(strcmp(catalog.items[i].title, reboot.items[i].title) == 0);
        while (catalog.count) assert(audio_catalog_delete_path(&catalog, 0));
        assert(audio_catalog_scan_path(&reboot, argv[2]));
        assert(reboot.healthy && reboot.count == 0);
    }
    printf("catalog %s: passed\n", argv[1]);
    return 0;
}
