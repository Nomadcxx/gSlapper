#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define main gslapper_program_main
#include "../src/main.c"
#undef main

static bool stop_request_priority(void) {
    int previous_pipe[2] = {wakeup_pipe[0], wakeup_pipe[1]};
    if (pipe(wakeup_pipe) < 0)
        return false;

    pending_stop_request = STOP_REQ_NONE;
    request_stop_from_thread(STOP_REQ_EXIT);
    request_stop_from_thread(STOP_REQ_RESTART);
    bool exit_wins = pending_stop_request == STOP_REQ_EXIT;

    close(wakeup_pipe[0]);
    close(wakeup_pipe[1]);
    wakeup_pipe[0] = previous_pipe[0];
    wakeup_pipe[1] = previous_pipe[1];
    pending_stop_request = STOP_REQ_NONE;
    return exit_wins;
}

int main(void) {
    char test_dir[] = "/tmp/gslapper-watch-list-XXXXXX";
    char marker[PATH_MAX];
    char entry[PATH_MAX + 16];
    if (!mkdtemp(test_dir) || snprintf(marker, sizeof(marker), "%s/marker", test_dir) >=
            (int)sizeof(marker) || snprintf(entry, sizeof(entry), "x$(touch %s)", marker) >=
            (int)sizeof(entry))
        return 1;

    char *list[] = {entry, NULL};
    check_watch_list(list);

    bool injected = access(marker, F_OK) == 0;
    unlink(marker);
    char *running_list[] = {"test-watch-list", NULL};
    bool detected = check_watch_list(running_list) == running_list[0];
    rmdir(test_dir);
    if (injected) {
        fprintf(stderr, "watch-list entry executed a shell command\n");
        return 1;
    }
    if (!detected) {
        fprintf(stderr, "watch-list failed to detect a running process\n");
        return 1;
    }
    if (!stop_request_priority()) {
        fprintf(stderr, "fatal stop request was overwritten by restart request\n");
        return 1;
    }
    return 0;
}
