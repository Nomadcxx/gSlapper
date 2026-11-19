#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <stdbool.h>
#include <stddef.h>

// A directory, or a regular file ending .m3u, .m3u8 or .txt (any case)
bool playlist_is_source(const char *path);

// Loads absolute, existing paths accepted by accept(). A directory keeps its accepted
// regular files sorted by name and skips the rest; a playlist file keeps its order,
// skips blank and '#' lines, resolves relative entries against its own directory, and
// fails on the first missing or rejected entry. On failure err describes why and
// nothing is allocated.
bool playlist_load(const char *path, bool (*accept)(const char *path),
                   char ***entries_out, size_t *count_out, char *err, size_t errlen);

void playlist_free(char **entries, size_t count);

#endif // PLAYLIST_H
