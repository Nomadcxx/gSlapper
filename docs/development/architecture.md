# Architecture

This document provides an overview of gSlapper's internal architecture and design decisions.

## Overview

gSlapper is a high-performance wallpaper manager for Wayland that combines the best of swww and mpvpaper. It uses GStreamer for media playback and EGL/OpenGL for rendering, providing faster performance, better efficiency, and fixes memory leaks on NVIDIA Wayland systems while offering superior multi-monitor support.

## Core Components

### main.c

The main application file (`src/main.c`, ~2000 lines) contains:

- **GStreamer pipeline management** - `init_gst()`, `buffer_probe()`, `bus_callback()`
- **EGL/OpenGL rendering** - `init_egl()`, `render()`, `create_shader_program()`
- **Wayland compositor integration** - `layer_surface_configure()`, `output_listener()`
- **Video texture management** - Smart allocation with `texture_manager` struct
- **Seamless looping** - Segment-based looping via `GST_MESSAGE_SEGMENT_DONE`
- **Multi-output support** - Independent rendering per monitor
- **Thread management** - Event handling and process monitoring

### holder.c

Process monitoring (`src/holder.c`, ~420 lines):

- Minimal Wayland client that monitors gslapper state
- Handles stoplist/pauselist functionality
- Revives main process when conditions are met
- Acts as a "gate keeper" before main application runs

### ipc.c/h

IPC control system (`src/ipc.c`, `inc/ipc.h`, ~500 lines):

- Unix domain socket server for runtime control
- Thread-safe command queue with mutex protection
- Client thread per connection, main thread processes commands
- Wakeup pipe integrates with main `poll()` loop
- Supports pause/resume/query/change/transition commands

### cflogprinter.c/h

Custom colored logging system:

- Functions: `cflp_info()`, `cflp_success()`, `cflp_warning()`, `cflp_error()`
- Provides consistent logging format across the application

### glad.c

OpenGL function loader (generated code):

- Provides EGL and OpenGL function pointers
- Enables runtime OpenGL function resolution

## Design Patterns

### Segment-Based Looping

Uses `GST_SEEK_FLAG_SEGMENT` instead of EOS-based looping:

- Eliminates playback gaps between loops
- Initialized on first `GST_STATE_PLAYING` transition
- Handled in `bus_callback()` (lines 976-1016)

### Smart Texture Management

Reuses texture allocation when dimensions match:

- Reduces GPU memory reallocations
- Prevents memory accumulation during playback
- Implemented in texture manager (lines 84-336)

### Thread-Safe Frame Updates

- `video_mutex` protects shared frame data
- `buffer_probe()` callback copies frame data asynchronously
- Wakeup pipe triggers rendering from main thread
- Frame updates (lines 881-945)

### EGL Context Management

Creates compatibility context first, falls back to core:

- Tries multiple OpenGL versions (3.3, 3.2, 4.x)
- Required for multi-output rendering
- Context management (lines 1326-1411)

### IPC Command Queue

- Thread-safe command queue with mutex protection
- Wakeup pipe integrates with main `poll()` loop
- Client threads handle socket I/O, main thread processes commands
- Responses sent back to clients via socket

## GStreamer Pipeline

### Pipeline Architecture

```
playbin → appsink (RGBA format) → buffer_probe → texture_manager → OpenGL rendering
```

### Options Handling

The `apply_gst_options()` function handles:

- `loop` - Enables segment-based seamless looping (videos only)
- `no-audio`/`mute` - Disables audio playback (videos only)
- `panscan=X` - Scales content by factor X (0.0-1.0, default for videos)
- `fill` - Fill screen maintaining aspect ratio, crop excess (default for images)
- `stretch` - Fill screen ignoring aspect ratio
- `original` - Display at actual pixel dimensions (1:1 mapping)
- Frame rate capping: 30/60/100 FPS options

### Image Support

Image pipeline:

```
filesrc → typefind → decoder → videoconvert → imagefreeze → appsink (RGBA)
```

- Format detection via file extension (case-insensitive)
- Uses GStreamer `imagefreeze` element for static display
- Default scaling mode: `fill` (crop to fill screen)
- Transitions: Supports fade transition between images (opt-in)

## Wayland Integration

### Layer Shell Protocol

- Uses `wlr-layer-shell-unstable-v1` protocol
- Supports background, bottom, top, overlay layers
- Handles multi-monitor configurations via `wl_output`

### Surface Management

- One `wl_surface` + `zwlr_layer_surface_v1` per output
- EGL window surface for each output
- Frame callbacks coordinate rendering with compositor

## Transition System

### Architecture

Transition state structure:

```c
typedef struct {
    transition_type_t type;      // TRANSITION_NONE or TRANSITION_FADE
    bool active;                 // Currently transitioning?
    bool enabled;                // Globally enabled?
    float duration;              // Total duration in seconds
    float progress;              // Progress 0.0-1.0
    uint8_t *old_pixels;         // Old image pixels (RGBA)
    uint8_t *new_pixels;         // New image pixels (RGBA)
    uint8_t *blend_pixels;       // Blended result for display
    int width, height;           // Display resolution
    struct timespec start_time;
} transition_state_t;
```

### Core Functions

1. `start_transition()` - Captures current wallpaper, prepares buffers
2. `activate_transition()` - Loads new image and starts blending
3. `update_transition()` - Blends pixels each frame using integer math
4. `complete_transition()` - Cleans up after completion
5. `cancel_transition()` - Cancels ongoing transition

### Blending Algorithm

CPU-based alpha blending for speed:

```c
// Integer-based alpha blending
int step = (int)(progress * 256.0f);
blend[i] = ((old[i] * (256 - step) + new[i] * step) >> 8);
```

## Memory Management

### Critical Cleanup Order

The `exit_cleanup()` function follows this order:

1. Signal threads to stop
2. Cancel all pthread threads
3. Clean up texture manager
4. Graceful GStreamer shutdown (PAUSED → READY → NULL)
5. Unref pipeline and bus
6. Destroy EGL contexts
7. Free allocated strings and arrays

### Memory Leak Prevention

- Always free GStreamer objects with `gst_object_unref()`
- Destroy Wayland objects in reverse creation order
- Use `g_free()` for GLib allocations, `free()` for malloc()

## Performance Considerations

### Frame Rate Management

- Default 30 FPS cap to reduce GPU load (`target_frame_time_ns`)
- Adaptive frame skipping under load
- Smart texture reallocation only when dimensions change

### GPU Memory Optimization

- `appsink` limited to 1 buffer (`max-buffers`)
- Texture updates use `glTexSubImage2D()`
- Frame rate limiting prevents buffer accumulation

### Multi-Monitor Efficiency

- Single pipeline with multiple rendering surfaces
- Shared texture across all outputs
- Independent rendering per monitor

## NVIDIA-Specific Handling

- Forces `GL_BACK` draw buffer for NVIDIA Pro drivers
- Swap interval 0 for immediate presentation
- Compatibility context preferred over core context

## Threading Model

### Main Thread

- Wayland event loop
- IPC command processing
- Rendering coordination
- GStreamer bus message handling

### Worker Threads

- **IPC client threads** - Handle individual socket connections
- **Pauselist monitor** - Monitors pauselist file changes
- **Stoplist monitor** - Monitors stoplist file changes

### Thread Communication

- `video_mutex` - Protects shared video frame data
- `ipc_queue_mutex` - Protects IPC command queue
- `wakeup_pipe` - Signals main thread for IPC commands
- Frame callbacks - Coordinate rendering with compositor

## File Structure

```
gSlapper/
├── src/
│   ├── main.c          # Main application (~2000 lines)
│   ├── holder.c        # Process monitoring (~420 lines)
│   ├── ipc.c           # IPC control system (~500 lines)
│   ├── cflogprinter.c  # Logging system
│   └── glad.c          # OpenGL loader
├── inc/
│   ├── ipc.h           # IPC interface
│   ├── cflogprinter.h  # Logging interface
│   └── glad/           # OpenGL headers
├── proto/
│   └── wlr-layer-shell-unstable-v1.xml  # Wayland protocol
└── docs/               # Documentation
```

## Future Improvements

- GPU-accelerated transitions (shader-based blending)
- Image preloading and caching
- Additional transition effects (wipe, center, outer, random)
- Hardware-accelerated video decoding
- Better error recovery and resilience
