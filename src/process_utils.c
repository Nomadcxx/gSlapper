#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

#include "process_utils.h"

extern char **environ;

// CHANGED 2026-10-26 - Use the native kernel interface for executable handoff.
int executable_path(char *buffer, size_t size) {
    if (!buffer || size == 0) {
        errno = EINVAL;
        return -1;
    }
#ifdef __FreeBSD__
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    size_t length = size;
    if (sysctl(mib, 4, buffer, &length, NULL, 0) < 0) {
        if (errno == ENOMEM)
            errno = ENAMETOOLONG;
        return -1;
    }
    if (length == 0) {
        errno = ENOENT;
        return -1;
    }
    if (length > size || buffer[length - 1] != '\0') {
        errno = ENAMETOOLONG;
        return -1;
    }
#else
    ssize_t length = readlink("/proc/self/exe", buffer, size);
    if (length < 0)
        return -1;
    if ((size_t)length >= size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    buffer[length] = '\0';
#endif
    return 0;
}

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
