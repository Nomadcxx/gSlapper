#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "../src/ipc.c"

static char test_dir[] = "/tmp/gslapper-ipc-test-XXXXXX";
static char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
static int server_fd = -1;

static void cleanup(void) {
    if (server_fd >= 0)
        close(server_fd);
    if (socket_path[0])
        unlink(socket_path);
    if (test_dir[0])
        rmdir(test_dir);
}

int main(void) {
    int failed = 0;
    struct stat st;

    if (!mkdtemp(test_dir) || snprintf(socket_path, sizeof(socket_path), "%s/socket", test_dir) >=
            (int)sizeof(socket_path))
        return 1;
    atexit(cleanup);

    mode_t old_umask = umask(0);
    server_fd = create_socket(socket_path);
    umask(old_umask);
    if (server_fd < 0 || stat(socket_path, &st) < 0 || (st.st_mode & 0777) != 0600) {
        fprintf(stderr, "IPC socket should be owner-only under umask 000\n");
        failed = 1;
    }

    int duplicate_fd = create_socket(socket_path);
    if (duplicate_fd >= 0) {
        fprintf(stderr, "active IPC socket should reject a second server\n");
        close(duplicate_fd);
        failed = 1;
    }

    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    int stale_fd = create_socket(socket_path);
    if (stale_fd >= 0) {
        fprintf(stderr, "stale IPC socket should not be unlinked automatically\n");
        close(stale_fd);
        failed = 1;
    }
    if (lstat(socket_path, &st) < 0 || !S_ISSOCK(st.st_mode)) {
        fprintf(stderr, "stale IPC socket path should remain untouched\n");
        failed = 1;
    }

    return failed;
}
