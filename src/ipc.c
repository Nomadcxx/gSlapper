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

// Stub implementations (will be replaced in later tasks)
bool ipc_init(const char *path) {
    (void)path;
    return false;
}

void ipc_shutdown(void) {
}

int ipc_get_wakeup_fd(void) {
    return ipc_wakeup_pipe[0];
}

ipc_command_t *ipc_dequeue_command(void) {
    return NULL;
}

void ipc_send_response(int client_fd, const char *response) {
    (void)client_fd;
    (void)response;
}

void ipc_drain_wakeup(void) {
}
