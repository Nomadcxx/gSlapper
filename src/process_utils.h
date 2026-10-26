#ifndef GSLAPPER_PROCESS_UTILS_H
#define GSLAPPER_PROCESS_UTILS_H

#include <stdbool.h>
#include <stddef.h>

int executable_path(char *buffer, size_t size);

bool process_is_running(const char *name);

#endif
