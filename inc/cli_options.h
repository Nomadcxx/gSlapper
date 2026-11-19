#ifndef CLI_OPTIONS_H
#define CLI_OPTIONS_H

#include <getopt.h>

// Shared by gslapper and gslapper-holder. The holder receives gslapper's full
// argv, so both must parse the same options or the holder misreads the output.
#define GSLAPPER_SHORT_OPTIONS "hdvfpsn:l:o:r:I:T:D:Z:SR"

#define GSLAPPER_LONG_OPTIONS                                   \
    {"help", no_argument, NULL, 'h'},                           \
    {"help-output", no_argument, NULL, 'd'},                    \
    {"verbose", no_argument, NULL, 'v'},                        \
    {"fork", no_argument, NULL, 'f'},                           \
    {"auto-pause", no_argument, NULL, 'p'},                     \
    {"auto-stop", no_argument, NULL, 's'},                      \
    {"slideshow", required_argument, NULL, 'n'},                \
    {"layer", required_argument, NULL, 'l'},                    \
    {"gst-options", required_argument, NULL, 'o'},              \
    {"fps-cap", required_argument, NULL, 'r'},                  \
    {"ipc-socket", required_argument, NULL, 'I'},               \
    {"transition-type", required_argument, NULL, 'T'},          \
    {"transition-duration", required_argument, NULL, 'D'},      \
    {"systemd", no_argument, NULL, 'S'},                        \
    {"restore", no_argument, NULL, 'R'},                        \
    {"save-state", no_argument, NULL, 1000},                    \
    {"state-file", required_argument, NULL, 1001},              \
    {"no-save-state", no_argument, NULL, 1002},                 \
    {"cache-size", required_argument, NULL, 1003},              \
    {"transition-angle", required_argument, NULL, 1004},        \
    {"transition-pos", required_argument, NULL, 1005},          \
    {0, 0, 0, 0}

#endif // CLI_OPTIONS_H
