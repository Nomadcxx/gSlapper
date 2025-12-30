#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>

#include "ipc.h"
#include "cflogprinter.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// Global state
static int listen_fd = -1;
static int shutdown_pipe[2] = {-1, -1};
static int ipc_wakeup_pipe[2] = {-1, -1};
static pthread_t server_thread = 0;
static char *socket_path_copy = NULL;

// Command queue
static ipc_command_t *cmd_queue_head = NULL;
static ipc_command_t *cmd_queue_tail = NULL;
static pthread_mutex_t ipc_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static void *ipc_server_thread_fn(void *arg);
static void *ipc_client_thread_fn(void *arg);

#define IPC_MAX_CMD_NAME_LEN 32
#define IPC_MAX_PATH_LEN 4096

static bool ipc_validate_input(const char *input, int client_fd) {
    if (!input || input[0] == '\0') {
        return false;
    }
    size_t len = strlen(input);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c < 0x20 && c != ' ' && c != '\t') {
            ipc_send_response(client_fd, "ERROR: invalid control character in input\n");
            return false;
        }
    }
    return true;
}

static void ipc_queue_command_internal(const char *cmd_line, int client_fd) {
    ipc_command_t *cmd = calloc(1, sizeof(ipc_command_t));
    if (!cmd) {
        cflp_error("Failed to allocate IPC command");
        return;
    }

    cmd->cmd_line = strdup(cmd_line);
    if (!cmd->cmd_line) {
        cflp_error("Failed to allocate IPC command string");
        free(cmd);
        return;
    }
    cmd->client_fd = client_fd;
    cmd->next = NULL;

    pthread_mutex_lock(&ipc_queue_mutex);
    if (cmd_queue_tail) {
        cmd_queue_tail->next = cmd;
        cmd_queue_tail = cmd;
    } else {
        cmd_queue_head = cmd_queue_tail = cmd;
    }
    pthread_mutex_unlock(&ipc_queue_mutex);
}

static int create_socket(const char *path) {
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        cflp_error("Failed to create IPC socket: %s", strerror(errno));
        return -1;
    }

    // Check if socket is in use by another instance before removing
    struct sockaddr_un test_addr = {0};
    test_addr.sun_family = AF_UNIX;
    strncpy(test_addr.sun_path, path, sizeof(test_addr.sun_path) - 1);
    if (connect(sock_fd, (struct sockaddr *)&test_addr, sizeof(test_addr)) == 0) {
        cflp_error("Another gslapper instance is using socket %s", path);
        close(sock_fd);
        return -1;
    }
    // Connection failed - socket is stale, safe to remove
    unlink(path);

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        cflp_error("Failed to bind IPC socket to %s: %s", path, strerror(errno));
        close(sock_fd);
        return -1;
    }

    if (listen(sock_fd, 5) < 0) {
        cflp_error("Failed to listen on IPC socket: %s", strerror(errno));
        close(sock_fd);
        return -1;
    }

    // Set close-on-exec
    fcntl(sock_fd, F_SETFD, FD_CLOEXEC);

    return sock_fd;
}

bool ipc_init(const char *path) {
    if (!path || path[0] == '\0') {
        cflp_error("Invalid IPC socket path");
        return false;
    }

    // Create shutdown pipe
    if (pipe(shutdown_pipe) < 0) {
        cflp_error("Failed to create IPC shutdown pipe: %s", strerror(errno));
        return false;
    }
    fcntl(shutdown_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(shutdown_pipe[1], F_SETFD, FD_CLOEXEC);

    // Create wakeup pipe
    if (pipe(ipc_wakeup_pipe) < 0) {
        cflp_error("Failed to create IPC wakeup pipe: %s", strerror(errno));
        close(shutdown_pipe[0]);
        close(shutdown_pipe[1]);
        shutdown_pipe[0] = shutdown_pipe[1] = -1;
        return false;
    }
    fcntl(ipc_wakeup_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(ipc_wakeup_pipe[1], F_SETFD, FD_CLOEXEC);

    // Make wakeup pipe non-blocking
    int flags = fcntl(ipc_wakeup_pipe[0], F_GETFL, 0);
    if (flags >= 0) {
        fcntl(ipc_wakeup_pipe[0], F_SETFL, flags | O_NONBLOCK);
    }
    flags = fcntl(ipc_wakeup_pipe[1], F_GETFL, 0);
    if (flags >= 0) {
        fcntl(ipc_wakeup_pipe[1], F_SETFL, flags | O_NONBLOCK);
    }

    // Create socket
    listen_fd = create_socket(path);
    if (listen_fd < 0) {
        close(shutdown_pipe[0]);
        close(shutdown_pipe[1]);
        close(ipc_wakeup_pipe[0]);
        close(ipc_wakeup_pipe[1]);
        shutdown_pipe[0] = shutdown_pipe[1] = -1;
        ipc_wakeup_pipe[0] = ipc_wakeup_pipe[1] = -1;
        return false;
    }

    // Save socket path for cleanup
    socket_path_copy = strdup(path);

    // Start server thread
    if (pthread_create(&server_thread, NULL, ipc_server_thread_fn, NULL) != 0) {
        cflp_error("Failed to create IPC server thread: %s", strerror(errno));
        close(listen_fd);
        close(shutdown_pipe[0]);
        close(shutdown_pipe[1]);
        close(ipc_wakeup_pipe[0]);
        close(ipc_wakeup_pipe[1]);
        listen_fd = -1;
        shutdown_pipe[0] = shutdown_pipe[1] = -1;
        ipc_wakeup_pipe[0] = ipc_wakeup_pipe[1] = -1;
        free(socket_path_copy);
        socket_path_copy = NULL;
        return false;
    }

    cflp_success("IPC server initialized on %s", path);
    return true;
}

void ipc_shutdown(void) {
    // Signal server thread to stop
    if (shutdown_pipe[1] >= 0) {
        if (write(shutdown_pipe[1], "x", 1) == -1) {
            cflp_warning("Failed to write to shutdown pipe: %s", strerror(errno));
        }
    }

    // Wait for server thread
    if (server_thread != 0) {
        pthread_join(server_thread, NULL);
        server_thread = 0;
    }

    // Close all pipes
    if (shutdown_pipe[0] >= 0) {
        close(shutdown_pipe[0]);
        shutdown_pipe[0] = -1;
    }
    if (shutdown_pipe[1] >= 0) {
        close(shutdown_pipe[1]);
        shutdown_pipe[1] = -1;
    }

    if (ipc_wakeup_pipe[0] >= 0) {
        close(ipc_wakeup_pipe[0]);
        ipc_wakeup_pipe[0] = -1;
    }
    if (ipc_wakeup_pipe[1] >= 0) {
        close(ipc_wakeup_pipe[1]);
        ipc_wakeup_pipe[1] = -1;
    }

    // Close listen socket
    if (listen_fd >= 0) {
        close(listen_fd);
        listen_fd = -1;
    }

    // Remove socket file
    if (socket_path_copy) {
        unlink(socket_path_copy);
        free(socket_path_copy);
        socket_path_copy = NULL;
    }

    // Clean up queued commands
    pthread_mutex_lock(&ipc_queue_mutex);
    ipc_command_t *cmd = cmd_queue_head;
    while (cmd) {
        ipc_command_t *next = cmd->next;
        free(cmd->cmd_line);
        free(cmd);
        cmd = next;
    }
    cmd_queue_head = cmd_queue_tail = NULL;
    pthread_mutex_unlock(&ipc_queue_mutex);

    cflp_info("IPC server shut down");
}

int ipc_get_wakeup_fd(void) {
    return ipc_wakeup_pipe[0];
}

ipc_command_t *ipc_dequeue_command(void) {
    pthread_mutex_lock(&ipc_queue_mutex);
    ipc_command_t *cmd = cmd_queue_head;
    if (cmd) {
        cmd_queue_head = cmd->next;
        if (!cmd_queue_head)
            cmd_queue_tail = NULL;
    }
    pthread_mutex_unlock(&ipc_queue_mutex);
    return cmd;
}

void ipc_send_response(int client_fd, const char *response) {
    if (client_fd < 0 || !response) return;
    size_t len = strlen(response);
    ssize_t sent = send(client_fd, response, len, MSG_NOSIGNAL);
    if (sent < 0 && errno != EPIPE) {
        cflp_warning("Failed to send IPC response: %s", strerror(errno));
    } else if ((size_t)sent < len) {
        cflp_warning("Partial IPC response sent: %zd/%zu bytes", sent, len);
    }
}

void ipc_drain_wakeup(void) {
    if (ipc_wakeup_pipe[0] < 0) return;
    char tmp[64];
    while (read(ipc_wakeup_pipe[0], tmp, sizeof(tmp)) > 0);
}

static void *ipc_client_thread_fn(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    sigset_t sigpipe_set;
    sigemptyset(&sigpipe_set);
    sigaddset(&sigpipe_set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &sigpipe_set, NULL);

    char buffer[4096] = {0};
    size_t buffer_len = 0;

    while (1) {
        char tmp[512];
        ssize_t bytes = recv(client_fd, tmp, sizeof(tmp), 0);
        if (bytes <= 0) break;

        if (buffer_len + bytes >= sizeof(buffer) - 1) {
            ipc_send_response(client_fd, "ERROR: command too long\n");
            break;
        }

        memcpy(buffer + buffer_len, tmp, bytes);
        buffer_len += bytes;
        buffer[buffer_len] = '\0';

        char *newline;
        while ((newline = strchr(buffer, '\n')) != NULL) {
            *newline = '\0';
            if (buffer[0] != '\0' && ipc_validate_input(buffer, client_fd)) {
                ipc_queue_command_internal(buffer, client_fd);
                if (ipc_wakeup_pipe[1] >= 0) {
                    if (write(ipc_wakeup_pipe[1], "c", 1) == -1 && errno != EAGAIN) {
                        cflp_warning("Failed to write to IPC wakeup pipe");
                    }
                }
            }
            size_t processed = (newline - buffer) + 1;
            memmove(buffer, newline + 1, buffer_len - processed);
            buffer_len -= processed;
        }
    }
    close(client_fd);
    return NULL;
}

static void *ipc_server_thread_fn(void *arg) {
    (void)arg;
    struct pollfd fds[2] = {
        {.events = POLLIN, .fd = shutdown_pipe[0]},
        {.events = POLLIN, .fd = listen_fd}
    };

    while (1) {
        int rc = poll(fds, 2, -1);
        if (rc < 0) {
            if (errno == EINTR) continue;
            cflp_error("IPC server poll error: %s", strerror(errno));
            break;
        }
        if (fds[0].revents & POLLIN) break;

        if (fds[1].revents & POLLIN) {
            int client_fd = accept(listen_fd, NULL, NULL);
            if (client_fd < 0) {
                if (errno != EINTR)
                    cflp_warning("Failed to accept IPC client: %s", strerror(errno));
                continue;
            }
            fcntl(client_fd, F_SETFD, FD_CLOEXEC);

            int *client_fd_ptr = malloc(sizeof(int));
            if (!client_fd_ptr) {
                cflp_error("Failed to allocate client FD");
                close(client_fd);
                continue;
            }
            *client_fd_ptr = client_fd;

            pthread_t client_thread;
            if (pthread_create(&client_thread, NULL, ipc_client_thread_fn, client_fd_ptr) == 0) {
                pthread_detach(client_thread);
            } else {
                cflp_error("Failed to create client thread: %s", strerror(errno));
                free(client_fd_ptr);
                close(client_fd);
            }
        }
    }
    return NULL;
}
