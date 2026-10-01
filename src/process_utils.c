#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>

#include "process_utils.h"

extern char **environ;

// CHANGED 2026-10-01 - Pass process names as arguments so config entries never reach a shell.
bool process_is_running(const char *name) {
    if (!name || name[0] == '\0')
        return false;

    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    if (error != 0)
        return false;

    error = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    if (error != 0) {
        posix_spawn_file_actions_destroy(&actions);
        return false;
    }

    char *const argv[] = {"pidof", "--", (char *)name, NULL};
    pid_t child;
    error = posix_spawnp(&child, "pidof", &actions, NULL, argv, environ);
    int destroy_error = posix_spawn_file_actions_destroy(&actions);
    if (error != 0)
        return false;

    int status;
    pid_t result;
    do {
        result = waitpid(child, &status, 0);
    } while (result < 0 && errno == EINTR);

    return destroy_error == 0 && result == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
