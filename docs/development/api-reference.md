# API Reference

This document provides a comprehensive reference for gSlapper's public APIs, internal functions, and data structures.

**Note**: This is a developer-facing API reference. For user-facing command-line options, see [Command Line Options](./command-line-options.md).

---

## Table of Contents

- [IPC API](#ipc-api)
- [State Management API](#state-management-api)
- [Logging API](#logging-api)
- [Internal APIs](#internal-apis)
  - [Wayland Integration](#wayland-integration)
  - [GStreamer Pipeline](#gstreamer-pipeline)
  - [Rendering](#rendering)
  - [Command-Line Parsing](#command-line-parsing)

---

## IPC API

The IPC (Inter-Process Communication) API allows runtime control of gSlapper via Unix domain sockets.

### Overview

The IPC system uses a client-server model:
- **Server**: Runs within gSlapper main process
- **Clients**: External processes (scripts, user tools)
- **Protocol**: Text-based commands over Unix socket

### Initialization

#### `ipc_init(const char *socket_path)`

Initialize IPC server with specified socket path.

**Parameters**:
- `socket_path` - Path to Unix socket (e.g., `/tmp/gslapper.sock`)

**Returns**:
- `true` on success
- `false` on failure

**Thread Safety**: Thread-safe (creates dedicated thread)

**Example**:
```c
#include "ipc.h"

bool success = ipc_init("/tmp/gslapper.sock");
if (!success) {
    fprintf(stderr, "Failed to initialize IPC server\n");
}
```

---

#### `ipc_shutdown(void)`

Shutdown IPC server and cleanup resources.

**Parameters**: None

**Returns**: void

**Thread Safety**: Thread-safe (uses internal locks)

**Behavior**:
- Closes all client connections
- Unlinks socket file
- Joins server thread
- Frees all allocated resources

**Example**:
```c
// At program exit or on signal
ipc_shutdown();
```

---

### Wakeup Mechanism

#### `ipc_get_wakeup_fd(void)`

Get file descriptor for the wakeup pipe (used by main event loop).

**Returns**:
- File descriptor (int) for wakeup pipe
- `-1` if IPC not initialized

**Purpose**:
- Allows main loop to monitor IPC activity via `poll()` or `select()`
- Writing to this pipe wakes up the main loop when new commands arrive

**Example**:
```c
#include <poll.h>

// In main event loop
struct pollfd fds[2];
fds[0].fd = wl_display_get_fd(display);
fds[0].events = POLLIN;
fds[1].fd = ipc_get_wakeup_fd();
fds[1].events = POLLIN;

while (running) {
    poll(fds, 2, -1);
    // Handle events...
}
```

---

#### `ipc_drain_wakeup(void)`

Drain the wakeup pipe (call before processing commands).

**Parameters**: None

**Returns**: void

**Purpose**:
- Clear pending data from wakeup pipe
- Prevents repeated wakeups for same event

**Example**:
```c
// After poll() returns with IPC event
if (fds[1].revents & POLLIN) {
    ipc_drain_wakeup();  // Clear pipe

    // Now process commands
    ipc_command_t *cmd = ipc_dequeue_command();
    // ...
}
```

---

### Command Queue

#### `ipc_dequeue_command(void)`

Dequeue next command from internal queue.

**Returns**:
- Pointer to `ipc_command_t` structure
- `NULL` if queue empty

**Responsibility**: Caller must free:
- `cmd->cmd_line` (string)
- Command structure itself (`free(cmd)`)

**Thread Safety**: Thread-safe (uses mutex-protected queue)

**Example**:
```c
while ((cmd = ipc_dequeue_command()) != NULL) {
    // Process cmd->cmd_line
    // Send response to cmd->client_fd
    ipc_send_response(cmd->client_fd, "OK\n");

    // Cleanup
    free(cmd->cmd_line);
    free(cmd);
}
```

---

#### `ipc_send_response(int client_fd, const char *response)`

Send response to IPC client (thread-safe).

**Parameters**:
- `client_fd` - File descriptor of client socket
- `response` - Response string to send

**Returns**: void

**Thread Safety**: Thread-safe

**Format**: Responses follow this format:
- Success: `OK` or `OK: <message>`
- Error: `ERROR: <message>`
- Query: `STATUS: <state> <type> <path>`
- Transition: `TRANSITION: <type> <enabled|disabled> <duration>`

**Example**:
```c
ipc_send_response(client_fd, "OK: Wallpaper changed\n");
ipc_send_response(client_fd, "ERROR: File not found\n");
ipc_send_response(client_fd, "STATUS: playing video /path/to/video.mp4\n");
```

---

### IPC Commands

#### Available Commands

| Command | Parameters | Response | Description |
|---------|------------|----------|-------------|
| `pause` | None | `OK` | Pause video playback |
| `resume` | None | `OK` | Resume paused playback |
| `stop` | None | `OK` | Stop gSlapper (exits) |
| `query` | None | `STATUS: <state> <type> <path>` | Get current wallpaper state |
| `change <path>` | File path | `OK` or `OK: transition started` | Change wallpaper |
| `layer <name>` | `background`, `bottom`, `top`, `overlay` | `OK` or `ERROR` | Change Wayland layer at runtime |
| `set-transition <type>` | `none` or `fade` | `OK` or `ERROR` | Set transition effect |
| `set-transition-duration <secs>` | 0.0-5.0 seconds | `OK` or `ERROR` | Set transition duration |
| `get-transition` | None | `TRANSITION: <type> <enabled> <duration>` | Query transition settings |

#### Example Client Code

```c
#include <sys/socket.h>
#include <sys/un.h>

int connect_to_ipc(const char *socket_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return -1;
    }
    return fd;
}

void send_ipc_command(int fd, const char *cmd) {
    write(fd, cmd, strlen(cmd));

    char response[1024];
    ssize_t n = read(fd, response, sizeof(response) - 1);
    if (n > 0) {
        response[n] = '\0';
        printf("Response: %s\n", response);
    }
}

int main() {
    int fd = connect_to_ipc("/tmp/gslapper.sock");
    if (fd < 0) {
        perror("Failed to connect");
        return 1;
    }

    send_ipc_command(fd, "pause\n");
    send_ipc_command(fd, "query\n");
    send_ipc_command(fd, "change /path/to/new/video.mp4\n");

    close(fd);
    return 0;
}
```

---

### Data Structures

#### `ipc_command_t`

```c
typedef struct ipc_command {
    char *cmd_line;          // Full command string from client
    int client_fd;            // Client socket file descriptor
    struct ipc_command *next;  // Next command in queue (linked list)
} ipc_command_t;
```

**Usage**:
- Internal to IPC system (queued by server thread)
- Dequeued by main thread for processing
- Must be freed by caller after processing

#### `preload_entry_t`

```c
typedef struct preload_entry {
    char *path;              // File path of preloaded image
    gpointer frame_data;       // GLib pointer to frame data
    gsize frame_size;         // Size of frame data in bytes
    gint width;               // Image width in pixels
    gint height;              // Image height in pixels
    struct preload_entry *next; // Next entry in cache (linked list)
} preload_entry_t;
```

**Note**: Currently reserved for future implementation (see TO_IMPLEMENT.md).

---

## State Management API

The state management API provides persistent storage and retrieval of wallpaper configuration.

### Overview

State is saved as simple key=value text files (not JSON):
- **Location**: `~/.local/state/gslapper/state-<output>.txt`
- **Format**: Human-readable text
- **Atomic writes**: Uses temp file + rename
- **Thread safety**: File locking prevents corruption

### Data Structures

#### `wallpaper_state`

```c
struct wallpaper_state {
    char *output;        // Monitor name (e.g., "DP-1", "DP-3")
    char *path;         // Full path to video or image file
    bool is_image;      // true = static image, false = video
    char *options;       // GStreamer options string (e.g., "loop panscan=0.8")
    double position;    // Video position in seconds (0.0 for images)
    bool paused;        // Pause state for videos (false by default)
};
```

**Example State File** (`state-DP-1.txt`):
```
version=1
output=DP-1
path=/home/user/Videos/wallpaper.mp4
is_image=false
options=loop panscan=0.8
position=123.45
paused=0
```

---

### Path Resolution

#### `get_state_file_path(const char *output_name)`

Generate state file path for given output.

**Parameters**:
- `output_name` - Monitor name (e.g., "DP-1"), or `NULL` for default

**Returns**:
- Dynamically allocated string (must be freed by caller)
- `NULL` on failure

**Path Resolution Logic**:
1. Check `XDG_STATE_HOME` environment variable
2. If not set, use `$HOME/.local/state/gslapper/`
3. Append `state-<output_name>.txt` (or `state.txt` if `NULL`)

**Example**:
```c
#include "state.h"

char *path = get_state_file_path("DP-1");
if (path) {
    printf("State file: %s\n", path);
    free(path);
}
```

---

### State File Operations

#### `save_state_file(const char *path, const struct wallpaper_state *state)`

Save wallpaper state to file.

**Parameters**:
- `path` - Full path to state file
- `state` - Pointer to `wallpaper_state` structure

**Returns**:
- `0` on success
- `-1` on failure

**Behavior**:
1. Create backup of existing file (if any)
2. Write to temporary file (`.tmp` suffix)
3. Verify write completed
4. Atomic rename (temp → actual file)
5. Set permissions to `0600` (user-only)
6. Use file locking to prevent concurrent writes

**Example**:
```c
struct wallpaper_state state = {
    .output = "DP-1",
    .path = "/home/user/Videos/wallpaper.mp4",
    .is_image = false,
    .options = "loop panscan=0.8",
    .position = 123.45,
    .paused = false
};

char *state_path = get_state_file_path("DP-1");
if (state_path && save_state_file(state_path, &state) == 0) {
    printf("State saved successfully\n");
}
free(state_path);
```

---

#### `load_state_file(const char *path, struct wallpaper_state *state)`

Load wallpaper state from file.

**Parameters**:
- `path` - Full path to state file
- `state` - Pointer to `wallpaper_state` structure to fill

**Returns**:
- `0` on success
- `-1` on failure (file not found, parse error, etc.)

**Behavior**:
1. Open and lock file (shared lock)
2. Read key=value pairs line by line
3. Parse and populate `state` structure
4. Validate file paths exist
5. Unlock and close file

**Parsed Keys**:
- `version` - State file version (currently 1)
- `output` - Monitor name
- `path` - Video/image file path
- `is_image` - Boolean (0 or 1)
- `options` - GStreamer options string
- `position` - Video position in seconds
- `paused` - Pause state (0 or 1)

**Example**:
```c
struct wallpaper_state state;
char *state_path = get_state_file_path("DP-1");

if (state_path && load_state_file(state_path, &state) == 0) {
    printf("Restoring: %s\n", state.path);
    printf("Options: %s\n", state.options);
    printf("Position: %.2f\n", state.position);
}
```

---

#### `free_wallpaper_state(struct wallpaper_state *state)`

Free all dynamically allocated memory in `wallpaper_state` structure.

**Parameters**:
- `state` - Pointer to `wallpaper_state` structure

**Returns**: void

**Behavior**:
- Frees `state->output`
- Frees `state->path`
- Frees `state->options`
- Does not free the structure itself (caller's responsibility)

**Example**:
```c
struct wallpaper_state *state = malloc(sizeof(struct wallpaper_state));
// ... fill state ...

// Free allocated strings
free_wallpaper_state(state);

// Free structure itself
free(state);
```

---

## Logging API

The logging API provides formatted output with color-coded severity levels.

### Functions

#### `cflp_success(char *msg, ...)`

Log success message in green.

**Parameters**:
- `msg` - Format string (printf-style)
- `...` - Variable arguments

**Color**: Green (`\033[0;32m`)

**Example**:
```c
#include "cflogprinter.h"

cflp_success("State saved to %s", state_path);
// Output (green): ✓ State saved to /home/user/.local/state/gslapper/state.txt
```

---

#### `cflp_error(char *msg, ...)`

Log error message in red.

**Parameters**:
- `msg` - Format string (printf-style)
- `...` - Variable arguments

**Color**: Red (`\033[1;31m`, bold)

**Example**:
```c
cflp_error("Failed to open file: %s", path);
// Output (red bold): ✗ Failed to open file: /path/to/file.txt
```

---

#### `cflp_warning(char *msg, ...)`

Log warning message in yellow.

**Parameters**:
- `msg` - Format string (printf-style)
- `...` - Variable arguments

**Color**: Yellow (`\033[0;33m`)

**Example**:
```c
cflp_warning("Video codec not found, falling back to software decoding");
// Output (yellow): ⚠ Video codec not found, falling back to software decoding
```

---

#### `cflp_info(char *msg, ...)`

Log informational message (no color by default).

**Parameters**:
- `msg` - Format string (printf-style)
- `...` - Variable arguments

**Color**: None (default terminal color)

**Example**:
```c
if (VERBOSE) {
    cflp_info("Initializing GStreamer pipeline...");
}
// Output (if VERBOSE): ℹ Initializing GStreamer pipeline...
```

---

#### `cflp_custom(char *color, char *msg, ...)`

Log message with custom color.

**Parameters**:
- `color` - ANSI color code string
- `msg` - Format string (printf-style)
- `...` - Variable arguments

**Available Colors**:
- `RED`, `GREEN`, `YELLOW`, `BLUE`, `MAGENTA`, `CYAN` (from `cflogprinter.h`)
- `RESET` - Reset to default terminal color

**Example**:
```c
cflp_custom(CYAN, "Loading image: %s", path);
cflp_custom(MAGENTA, "Status: %s", status);
cflp_custom(RESET, "Back to normal color");
```

---

### Color Constants

```c
#define RED         "\033[1;31m"    // Bold red
#define GREEN       "\033[0;32m"    // Green
#define YELLOW      "\033[0;33m"    // Yellow
#define BLUE        "\033[0;34m"    // Blue
#define MAGENTA     "\033[0;35m"    // Magenta
#define CYAN        "\033[0;36m"    // Cyan
#define RESET       "\033[0m"       // Reset to default
```

---

## Internal APIs

This section documents internal functions and structures used by gSlapper's main implementation.

### Wayland Integration

#### Global State Structure

```c
struct wl_state {
    struct wl_display *display;           // Wayland display connection
    struct wl_compositor *compositor;     // Compositor interface
    struct zwlr_layer_shell_v1 *layer_shell; // Layer shell protocol
    struct wl_list outputs;               // List of display_output structures
    char *monitor;                       // User-selected output name (or '*')
    int surface_layer;                    // Layer level (background, bottom, top, overlay)
};
```

---

#### Display Output Structure

```c
struct display_output {
    uint32_t wl_name;                  // Wayland output name (numeric)
    struct wl_output *wl_output;         // Output interface
    char *name;                         // Output name string (e.g., "DP-1")
    char *identifier;                    // Output model/serial string

    struct wl_state *state;              // Parent state reference
    struct wl_surface *surface;           // Wayland surface
    struct zwlr_layer_surface_v1 *layer_surface; // Layer surface
    struct wl_egl_window *egl_window;    // EGL window
    EGLSurface *egl_surface;             // EGL surface

    uint32_t width, height;             // Output dimensions
    uint32_t scale;                     // Output scale factor (DPI)

    struct wl_list link;                 // Link in state.outputs list

    struct wl_callback *frame_callback;   // Frame callback
    bool redraw_needed;                  // Flag for frame request
};
```

---

### GStreamer Pipeline

#### Global GStreamer Elements

```c
static GstElement *pipeline;             // Main GStreamer pipeline
static GstBus *bus;                   // Pipeline bus for events/messages
static char *allocated_uri;            // URI for current media (for cleanup)
static bool using_waylandsink;        // Whether using waylandsink for direct rendering
```

---

#### Video Texture Management

```c
static GLuint video_texture = 0;       // OpenGL texture for video frames
static GLuint shader_program = 0;       // Default shader program
static GLuint transition_shader_program = 0; // Transition effect shader
static GLuint vao = 0, vbo = 0;    // Vertex array/buffer objects

// Smart texture management to reduce reallocations
static struct {
    GLuint texture;
    int current_width;
    int current_height;
    gboolean initialized;
} texture_manager = {0};

// Video frame data for thread-safe texture updates
static struct {
    gboolean has_new_frame;
    gint width, height;
    gpointer data;
    gsize size;
    gint last_width, last_height;     // Track dimension changes
} video_frame_data = {0};
```

---

#### Frame Rate Limiting

```c
static struct timespec last_render_time = {0};
static long long target_frame_time_ns = 33333333;  // Default ~30 FPS
static int frame_rate_cap = 30;                   // Default 30 FPS
static int frames_skipped = 0;                     // Adaptive skipping
```

**Purpose**: Reduce GPU usage by limiting rendering rate.

**Default**: 30 FPS (adjustable via `-r` CLI flag)

---

### State Management Globals

```c
static char *state_file_path = NULL;
static bool systemd_mode = false;
static bool save_state_on_exit = true;
static bool restore_flag = false;
static bool save_state_flag = false;

static double restore_position = 0.0;
static bool restore_paused = false;
```

---

### Transition Effects

```c
typedef enum {
    TRANSITION_NONE = 0,
    TRANSITION_FADE
} transition_type_t;

typedef struct {
    transition_type_t type;       // Transition effect type
    bool active;                 // Currently in transition
    bool enabled;                // Transitions globally enabled
    float duration;              // Transition duration in seconds
    float elapsed;               // Time since transition started
    float progress;              // 0.0 to 1.0 (0% to 100%)

    GLuint old_texture;          // Previous wallpaper texture
    GLuint new_texture;          // New wallpaper texture
    int old_width, old_height;  // Old wallpaper dimensions
    int new_width, new_height;  // New wallpaper dimensions
    float old_scale_x, old_scale_y; // Old scaling factors
    float alpha_old, alpha_new;   // Blend factors

    struct timespec start_time;    // Transition start time
} transition_state_t;
```

**Note**: Transitions only work between static images, not videos.

---

### Configuration Management

```c
static struct {
    char **pauselist;          // List of app names for auto-pause
    char **stoplist;           // List of app names for auto-stop

    int argc;                   // Saved argc for restart
    char **argv_copy;           // Saved argv for restart
    char *save_info;            // Saved command info

    bool auto_pause;             // Auto-pause when wallpaper hidden
    bool auto_stop;              // Auto-stop when wallpaper hidden

    int is_paused;              // Current pause state
    bool frame_ready;            // New frame available flag
    bool stop_render_loop;        // Flag to stop rendering loop
} app_state = {0};
```

---

## Command-Line Parsing

### Supported Options

| Short | Long | Type | Description |
|-------|-------|-------|-------------|
| `-h` | `--help` | flag | Display help message |
| `-v` | `--verbose` | flag | Be more verbose (-vv for higher verbosity) |
| `-f` | `--fork` | flag | Fork to background after starting |
| `-d` | `--help-output` | flag | Display all available outputs and quit |
| `-p` | `--auto-pause` | flag | Auto-pause when wallpaper hidden |
| `-s` | `--auto-stop` | flag | Auto-stop when wallpaper hidden |
| `-n` | `--slideshow` | seconds | Slideshow mode (TODO: not fully implemented) |
| `-l` | `--layer` | string | Shell surface layer (background, bottom, top, overlay) |
| `-o` | `--gst-options` | string | GStreamer options (see below) |
| `-r` | `--fps-cap` | int | Frame rate cap (30, 60, or 100 FPS) |
| `-I` | `--ipc-socket` | path | Enable IPC control via Unix socket |
| | `--transition-type` | string | Transition effect (none, fade) |
| | `--transition-duration` | float | Transition duration in seconds (default: 0.5) |
| `-S` | `--systemd` | flag | Enable systemd service mode |
| `-R` | `--restore` | flag | Restore wallpaper from saved state |
| | `--save-state` | flag | Save current state and exit |
| | `--state-file` | path | Use custom state file path |
| | `--no-save-state` | flag | Disable automatic state saving on exit |

### GStreamer Options (passed via `-o`)

| Option | Description | Default |
|--------|-------------|----------|
| `loop` | Seamless video looping | Off |
| `fill` | Fill screen maintaining aspect ratio (crop excess) | Default for images |
| `panscan=X` | Fit inside screen (0.0-1.0 scale factor) | 1.0 (full) |
| `stretch` | Fill screen ignoring aspect ratio | Off |
| `original` | Display at native resolution | Off |
| `no-audio` | Disable audio playback | Off |

---

## Helper Programs

### holder.c

**Purpose**: Gatekeeper process that checks stoplist conditions before main program runs.

**Behavior**:
1. Reads stoplist from `~/.config/mpvpaper/stoplist`
2. Monitors running applications
3. Allows main program to run only if no stoplist app is active
4. Manages process lifecycle transitions

**Usage**: Automatically invoked by main gSlapper binary (not user-facing).

---

## Thread Safety

### Mutexes

| Mutex | Purpose |
|--------|---------|
| `video_mutex` | Protects `video_frame_data` and texture updates |
| `state_mutex` | Protects state file operations |
| IPC queue lock | Protects command queue (internal to ipc.c) |

### Thread Model

gSlapper uses multi-threading:
- **Main Thread**: Wayland event loop, rendering, IPC command processing
- **GStreamer Bus Thread**: Handles pipeline events (EOM, errors, state changes)
- **Pause/Stop Monitor Thread**: Checks stoplist/pauselist conditions
- **IPC Server Thread**: Accepts client connections and enqueues commands

---

## Error Handling

### Common Return Values

| Value | Meaning |
|--------|---------|
| `0` | Success |
| `-1` | Failure (check errno) |
| `NULL` | Failure (check logs) |
| `false` | Failure |
| `true` | Success |

### Error Logging

All errors are logged via `cflp_error()` with descriptive messages.

**Common Error Scenarios**:
- **Wayland connection failed**: Check `WAYLAND_DISPLAY` environment variable
- **File not found**: Check path in state file
- **GStreamer pipeline error**: Check codec support, install `gst-plugins-ugly`
- **IPC socket error**: Check socket path, permissions

---

## Building with these APIs

### Example: Custom Wallpaper Manager

```c
#include "ipc.h"
#include <stdio.h>

int main() {
    // Start gSlapper with IPC
    system("gslapper -I /tmp/gslapper.sock -o 'loop' DP-1 /path/to/video.mp4 &");
    sleep(2);  // Wait for startup

    // Connect to IPC
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/gslapper.sock", sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Connect failed");
        return 1;
    }

    // Interactive control
    char input[256];
    while (1) {
        printf("> ");
        fgets(input, sizeof(input), stdin);

        if (strcmp(input, "pause\n") == 0) {
            write(fd, "pause\n", 6);
        } else if (strcmp(input, "resume\n") == 0) {
            write(fd, "resume\n", 7);
        } else if (strcmp(input, "quit\n") == 0) {
            write(fd, "stop\n", 5);
            break;
        }

        // Read response
        char response[1024];
        ssize_t n = read(fd, response, sizeof(response) - 1);
        if (n > 0) {
            response[n] = '\0';
            printf("%s", response);
        }
    }

    close(fd);
    return 0;
}
```

### Example: State-Based Wallpaper Switcher

```c
#include "state.h"
#include <dirent.h>

void switch_wallpaper(const char *new_path) {
    struct wallpaper_state state;
    char *state_path = get_state_file_path("DP-1");

    // Load current state
    if (state_path && load_state_file(state_path, &state) == 0) {
        // Update path
        free(state.path);
        state.path = strdup(new_path);

        // Save state
        if (save_state_file(state_path, &state) == 0) {
            printf("State updated: %s\n", new_path);
        }
    }

    free(state_path);
}
```

---

## See Also

- [Architecture Documentation](../development/architecture.md) - Detailed architecture overview
- [Command Line Options](./command-line-options.md) - User-facing CLI reference
- [IPC Control Guide](./ipc-control.md) - Using IPC from scripts
- [Persistent Wallpapers](./persistent-wallpapers.md) - State management usage
- [TO_IMPLEMENT.md](../../TO_IMPLEMENT.md) - Planned features and API additions
