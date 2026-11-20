#include <getopt.h>
#include <string.h>

#define main holder_program_main
#include "../src/holder.c"
#undef main

static int parses_monitor(int argc, char **argv, const char *expected, int auto_stop) {
    struct wl_state state = {0};
    halt_info.auto_stop = 0;
    optind = 1;
    parse_command_line(argc, argv, &state);
    int ok = state.monitor && strcmp(state.monitor, expected) == 0 && halt_info.auto_stop == auto_stop;
    free(state.monitor);
    return ok;
}

int main(void) {
    char *short_transition[] = {"gslapper-holder", "-s", "-T", "fade", "-D", "1.0", "DP-1", "/v.mp4"};
    if (!parses_monitor(8, short_transition, "DP-1", 1))
        return 1;

    char *state_options[] = {"gslapper-holder", "-s", "--state-file", "/x/state.txt",
                             "--no-save-state", "-S", "DP-2", "/v.mp4"};
    if (!parses_monitor(8, state_options, "DP-2", 1))
        return 2;

    char *restore[] = {"gslapper-holder", "--restore", "--systemd", "*", "/v.mp4"};
    if (!parses_monitor(5, restore, "*", 0))
        return 3;

    return 0;
}
