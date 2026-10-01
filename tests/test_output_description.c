#include <stdlib.h>
#include <string.h>

#define main holder_program_main
#include "../src/holder.c"
#undef main

static void test_wildcard_with_missing_output_fields(void) {
    struct wl_state state = {.monitor = "*"};
    struct display_output output = {0};
    output.state = &state;
    output.layer_surface = (struct zwlr_layer_surface_v1 *)1;

    output_done(&output, NULL);
}

static int test_monitor_matching(void) {
    return output_matches_monitor("DP-1", NULL, NULL) ||
            !output_matches_monitor("*", NULL, NULL) ||
            output_matches_monitor("DP-1", NULL, "") ||
            !output_matches_monitor("Dell U2724D", "DP-1", "Dell U2724D");
}

int main(void) {
    struct display_output output = {0};

    output_description(&output, NULL, "(DP-1)");
    if (!output.identifier || strcmp(output.identifier, "") != 0)
        return 1;
    free(output.identifier);

    output.identifier = NULL;
    output_description(&output, NULL, "Dell U2724D (DP-1)");
    if (!output.identifier || strcmp(output.identifier, "Dell U2724D") != 0)
        return 1;
    free(output.identifier);

    output.identifier = NULL;
    output_description(&output, NULL, "Laptop panel");
    if (!output.identifier || strcmp(output.identifier, "Laptop panel") != 0)
        return 1;
    free(output.identifier);

    if (test_monitor_matching())
        return 1;
    test_wildcard_with_missing_output_fields();
    return 0;
}
