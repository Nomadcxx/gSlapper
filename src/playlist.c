#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "playlist.h"

typedef struct {
    char **items;
    size_t count, capacity;
} path_list;

static bool list_push(path_list *list, const char *path) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 16;
        char **items = realloc(list->items, capacity * sizeof(char *));
        if (!items)
            return false;
        list->items = items;
        list->capacity = capacity;
    }
    char *copy = strdup(path);
    if (!copy)
        return false;
    list->items[list->count++] = copy;
    return true;
}

void playlist_free(char **entries, size_t count) {
    for (size_t i = 0; i < count; i++)
        free(entries[i]);
    free(entries);
}

static bool has_playlist_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    return dot && (strcasecmp(dot, ".m3u") == 0 || strcasecmp(dot, ".m3u8") == 0 ||
                   strcasecmp(dot, ".txt") == 0);
}

bool playlist_is_source(const char *path) {
    struct stat st;
    if (!path || stat(path, &st) != 0)
        return false;
    return S_ISDIR(st.st_mode) || (S_ISREG(st.st_mode) && has_playlist_extension(path));
}

static int compare_paths(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static bool load_directory(const char *dir_path, bool (*accept)(const char *), path_list *list,
                           char *err, size_t errlen) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        snprintf(err, errlen, "%s: %s", dir_path, strerror(errno));
        return false;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char joined[PATH_MAX], resolved[PATH_MAX];
        struct stat st;
        if (entry->d_name[0] == '.')
            continue;
        if (snprintf(joined, sizeof(joined), "%s/%s", dir_path, entry->d_name) >= (int)sizeof(joined))
            continue;
        if (stat(joined, &st) != 0 || !S_ISREG(st.st_mode) || !accept(joined))
            continue;
        if (!realpath(joined, resolved) || !list_push(list, resolved)) {
            closedir(dir);
            snprintf(err, errlen, "%s: %s", joined, strerror(errno ? errno : ENOMEM));
            return false;
        }
    }
    closedir(dir);
    qsort(list->items, list->count, sizeof(char *), compare_paths);
    return true;
}

static bool load_file(const char *file_path, bool (*accept)(const char *), path_list *list,
                      char *err, size_t errlen) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        snprintf(err, errlen, "%s: %s", file_path, strerror(errno));
        return false;
    }
    char base[PATH_MAX];
    snprintf(base, sizeof(base), "%s", file_path);
    char *slash = strrchr(base, '/');
    if (slash)
        *slash = '\0';
    else
        snprintf(base, sizeof(base), ".");

    char line[PATH_MAX];
    bool ok = true;
    bool first_line = true;
    while (ok && fgets(line, sizeof(line), file)) {
        char *start = line;
        // CHANGED 2026-11-20 - Skip a UTF-8 BOM before the first entry - Problem: "UTF-8 with BOM" .m3u8 files turned the BOM into part of the first path and failed at startup
        if (first_line && strncmp(start, "\xEF\xBB\xBF", 3) == 0)
            start += 3;
        first_line = false;
        while (isspace((unsigned char)*start))
            start++;
        char *end = start + strlen(start);
        while (end > start && isspace((unsigned char)end[-1]))
            *--end = '\0';
        if (*start == '\0' || *start == '#')
            continue;

        char joined[PATH_MAX], resolved[PATH_MAX];
        if (start[0] == '/')
            snprintf(joined, sizeof(joined), "%s", start);
        else if (snprintf(joined, sizeof(joined), "%s/%s", base, start) >= (int)sizeof(joined)) {
            snprintf(err, errlen, "%s: path too long", start);
            ok = false;
            break;
        }
        if (!realpath(joined, resolved)) {
            snprintf(err, errlen, "%s: %s", start, strerror(errno));
            ok = false;
        } else if (!accept(resolved)) {
            snprintf(err, errlen, "%s: slideshow supports static images only", start);
            ok = false;
        } else if (!list_push(list, resolved)) {
            snprintf(err, errlen, "%s: out of memory", start);
            ok = false;
        }
    }
    fclose(file);
    return ok;
}

bool playlist_load(const char *path, bool (*accept)(const char *path),
                   char ***entries_out, size_t *count_out, char *err, size_t errlen) {
    path_list list = {0};
    struct stat st;
    bool ok;

    if (stat(path, &st) != 0) {
        snprintf(err, errlen, "%s: %s", path, strerror(errno));
        return false;
    }
    if (S_ISDIR(st.st_mode))
        ok = load_directory(path, accept, &list, err, errlen);
    else
        ok = load_file(path, accept, &list, err, errlen);

    if (ok && list.count == 0) {
        snprintf(err, errlen, "no static images found in %s", path);
        ok = false;
    }
    if (!ok) {
        playlist_free(list.items, list.count);
        return false;
    }
    *entries_out = list.items;
    *count_out = list.count;
    return true;
}
