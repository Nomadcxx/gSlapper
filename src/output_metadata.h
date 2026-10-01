#ifndef GSLAPPER_OUTPUT_METADATA_H
#define GSLAPPER_OUTPUT_METADATA_H

#include <stdbool.h>

char *output_identifier_from_description(const char *description);
bool output_matches_monitor(const char *monitor, const char *name, const char *identifier);

#endif
