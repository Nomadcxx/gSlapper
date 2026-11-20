#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "playlist.h"

static int failures = 0;
static char root[PATH_MAX];

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            failures++;                                                 \
        }                                                               \
    } while (0)

static bool accept_png(const char *path) {
    size_t len = strlen(path);
    return len > 4 && strcmp(path + len - 4, ".png") == 0;
}

static void touch(const char *rel, const char *content) {
    char path[PATH_MAX + 64];
    snprintf(path, sizeof(path), "%s/%s", root, rel);
    FILE *f = fopen(path, "w");
    fputs(content, f);
    fclose(f);
}

static const char *at(const char *rel) {
    static char path[PATH_MAX + 64];
    snprintf(path, sizeof(path), "%s/%s", root, rel);
    return path;
}

static void test_directory(void) {
    char **entries = NULL;
    size_t count = 0;
    char err[256] = "";
    EXPECT(playlist_load(at("dir"), accept_png, &entries, &count, err, sizeof(err)));
    EXPECT(count == 2);
    if (count == 2) {
        EXPECT(strcmp(entries[0], at("dir/a.png")) == 0);
        EXPECT(strcmp(entries[1], at("dir/b.png")) == 0);
    }
    playlist_free(entries, count);
}

static void test_playlist_file(void) {
    char content[PATH_MAX * 2];
    snprintf(content, sizeof(content), "# comment\n\n  b.png  \n%s\n", at("dir/a.png"));
    touch("dir/list.m3u", content);
    char **entries = NULL;
    size_t count = 0;
    char err[256] = "";
    EXPECT(playlist_load(at("dir/list.m3u"), accept_png, &entries, &count, err, sizeof(err)));
    EXPECT(count == 2);
    if (count == 2) {
        EXPECT(strcmp(entries[0], at("dir/b.png")) == 0);
        EXPECT(strcmp(entries[1], at("dir/a.png")) == 0);
    }
    playlist_free(entries, count);
}

static void test_playlist_bom(void) {
    // Editors that save .m3u8 as "UTF-8 with BOM" put EF BB BF before the first entry
    touch("dir/bom.m3u8", "\xEF\xBB\xBF" "b.png\n");
    char **entries = NULL;
    size_t count = 0;
    char err[256] = "";
    EXPECT(playlist_load(at("dir/bom.m3u8"), accept_png, &entries, &count, err, sizeof(err)));
    EXPECT(count == 1);
    if (count == 1)
        EXPECT(strcmp(entries[0], at("dir/b.png")) == 0);
    playlist_free(entries, count);
}

static void test_rejected_entry(void) {
    touch("dir/clip.mp4", "");
    touch("dir/bad.txt", "a.png\nclip.mp4\n");
    char **entries = NULL;
    size_t count = 0;
    char err[256] = "";
    EXPECT(!playlist_load(at("dir/bad.txt"), accept_png, &entries, &count, err, sizeof(err)));
    EXPECT(strstr(err, "clip.mp4: slideshow supports static images only") != NULL);

    touch("dir/missing.txt", "nothere.png\n");
    EXPECT(!playlist_load(at("dir/missing.txt"), accept_png, &entries, &count, err, sizeof(err)));
    EXPECT(strstr(err, "nothere.png: ") != NULL);
}

static void test_empty_directory(void) {
    char **entries = NULL;
    size_t count = 0;
    char err[256] = "";
    EXPECT(!playlist_load(at("empty"), accept_png, &entries, &count, err, sizeof(err)));
    EXPECT(strncmp(err, "no static images found", 22) == 0);
}

static void test_is_source(void) {
    touch("x.M3U", "");
    EXPECT(playlist_is_source(at("dir")));
    EXPECT(playlist_is_source(at("x.M3U")));
    EXPECT(!playlist_is_source(at("dir/a.png")));
    EXPECT(!playlist_is_source(at("does-not-exist.txt")));
}

int main(void) {
    char tmpl[] = "/tmp/gsl-playlist-XXXXXX";
    if (!mkdtemp(tmpl))
        return 2;
    if (!realpath(tmpl, root))
        return 2;
    mkdir(at("dir"), 0700);
    mkdir(at("dir/sub"), 0700);
    mkdir(at("empty"), 0700);
    touch("dir/b.png", "");
    touch("dir/a.png", "");
    touch("dir/notes.txt", "");

    test_directory();
    test_playlist_file();
    test_playlist_bom();
    test_rejected_entry();
    test_empty_directory();
    test_is_source();

    char cmd[PATH_MAX + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
    if (system(cmd) != 0)
        fprintf(stderr, "cleanup failed for %s\n", root);
    return failures != 0;
}
