#ifndef IPC_H
#define IPC_H

#include <stdbool.h>

// Command structure for queue
typedef struct ipc_command {
    char *cmd_line;
    int client_fd;
    struct ipc_command *next;
} ipc_command_t;

// Initialize IPC server with socket path
// Returns true on success, false on failure
bool ipc_init(const char *socket_path);

// Shutdown IPC server and cleanup
void ipc_shutdown(void);

// Get wakeup pipe FD for main loop poll()
// Returns -1 if IPC not initialized
int ipc_get_wakeup_fd(void);

// Dequeue next command (caller must free cmd_line and struct)
// Returns NULL if queue empty
ipc_command_t *ipc_dequeue_command(void);

// Send response to client (thread-safe)
void ipc_send_response(int client_fd, const char *response);

// Drain the wakeup pipe (call before processing commands)
void ipc_drain_wakeup(void);

#endif // IPC_H
