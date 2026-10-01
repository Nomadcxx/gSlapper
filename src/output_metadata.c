#include <stdlib.h>
#include <string.h>

#include <stdbool.h>

#include "output_metadata.h"

char *output_identifier_from_description(const char *description) {
    const char *paren = strrchr(description, '(');
    size_t length = paren ? (size_t)(paren - description) : strlen(description);
    if (paren && length > 0 && description[length - 1] == ' ')
        length--;

    char *identifier = malloc(length + 1);
    if (!identifier)
        return NULL;

    memcpy(identifier, description, length);
    identifier[length] = '\0';
    return identifier;
}

bool output_matches_monitor(const char *monitor, const char *name, const char *identifier) {
    if (!monitor)
        return false;
    if (name && strstr(monitor, name))
        return true;
    if (identifier && identifier[0] && strstr(monitor, identifier))
        return true;

    return strcmp(monitor, "*") == 0 || strcmp(monitor, "ALL") == 0 ||
            strcmp(monitor, "All") == 0 || strcmp(monitor, "all") == 0;
}
