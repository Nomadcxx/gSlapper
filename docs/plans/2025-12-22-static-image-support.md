# Static Image Support Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add static image wallpaper support (JPEG, PNG, WebP, GIF, JXL) to achieve feature parity with swww/hyprpaper while maintaining gSlapper's unified video+image approach.

**Architecture:** Hybrid file detection (extension check + GStreamer fallback). Separate GStreamer pipelines for video vs image (playbin3 for video, simple filesrc->decodebin->appsink for images). Shared OpenGL rendering infrastructure. New `fill` scaling mode as default for images.

**Tech Stack:** GStreamer (decodebin, videoconvert, appsink), OpenGL (existing texture manager), POSIX (file extension parsing).

---

## Task 1: Add Image Mode Global State

**Files:**
- Modify: `src/main.c:112` (after stretch_mode declaration)

**Step 1: Add image mode state variables**

Find line 112 (`static bool stretch_mode = false;`) and add after it:

```c
static bool fill_mode = false;       // Fill screen maintaining aspect ratio (crops excess)
static bool is_image_mode = false;   // True if displaying static image vs video
static bool image_frame_captured = false;  // True once image frame is decoded
```

**Step 2: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors

**Step 3: Commit**

```bash
git add src/main.c
git commit -m "Add image mode state variables"
```

---

## Task 2: Add Image Extension Detection Function

**Files:**
- Modify: `src/main.c` (add before `init_gst` function around line 1290)

**Step 1: Add extension detection function**

Add this function before `init_gst`:

```c
// Known image extensions for fast-path detection
static const char *image_extensions[] = {
    ".jpg", ".jpeg", ".png", ".webp", ".gif", ".jxl", NULL
};

static bool is_image_file(const char *path) {
    if (!path) return false;

    // Get file extension (find last dot)
    const char *ext = strrchr(path, '.');
    if (!ext) return false;

    // Convert to lowercase for comparison
    char ext_lower[16] = {0};
    for (int i = 0; ext[i] && i < 15; i++) {
        ext_lower[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
    }

    // Check against known image extensions
    for (int i = 0; image_extensions[i]; i++) {
        if (strcmp(ext_lower, image_extensions[i]) == 0) {
            return true;
        }
    }

    return false;
}
```

**Step 2: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors (warning about unused function is OK)

**Step 3: Commit**

```bash
git add src/main.c
git commit -m "Add image extension detection function"
```

---

## Task 3: Add Fill Mode to update_vertex_data

**Files:**
- Modify: `src/main.c:475-505` (the scaling mode logic in update_vertex_data)

**Step 1: Add fill mode logic**

Find the `} else if (stretch_mode) {` block (around line 475) and add a new condition BEFORE it:

```c
    } else if (fill_mode) {
        // Fill mode - fill entire screen maintaining aspect ratio (crop excess)
        float video_aspect = (float)video_frame_data.width / (float)video_frame_data.height;
        float display_aspect = (float)output->width / (float)output->height;

        // Start with full coverage
        scale_x = 1.0f;
        scale_y = 1.0f;

        if (video_aspect > display_aspect) {
            // Video/image is wider than display - scale based on height, crop width
            scale_x = video_aspect / display_aspect;
        } else {
            // Video/image is taller than display - scale based on width, crop height
            scale_y = display_aspect / video_aspect;
        }

        if (VERBOSE) {
            cflp_info("Fill mode: scale_x=%.3f, scale_y=%.3f (video_aspect=%.3f, display_aspect=%.3f)",
                     scale_x, scale_y, video_aspect, display_aspect);
        }
    } else if (stretch_mode) {
```

The complete block should now read: `if (panscan_value == -1.0f) { ... } else if (fill_mode) { ... } else if (stretch_mode) { ... } else { ... }`

**Step 2: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors

**Step 3: Commit**

```bash
git add src/main.c
git commit -m "Add fill scaling mode for images"
```

---

## Task 4: Add Fill Option Parsing

**Files:**
- Modify: `src/main.c` (in apply_gst_options or option parsing section)

**Step 1: Find the stretch option parsing**

Search for where `stretch_mode = true` is set (around line 1245). Add fill mode parsing nearby.

Run: `grep -n "stretch_mode = true" src/main.c`

**Step 2: Add fill mode parsing**

Find the option parsing section and add fill mode handling. Look for `strstr(gst_options, "stretch")` and add before it:

```c
    // Check for fill mode (fill screen maintaining aspect, crop excess)
    if (strstr(gst_options, "fill") != NULL) {
        fill_mode = true;
        if (VERBOSE)
            cflp_info("Fill mode enabled (crop to fill screen)");
    }
```

**Step 3: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors

**Step 4: Commit**

```bash
git add src/main.c
git commit -m "Add fill option parsing"
```

---

## Task 5: Create Image Pipeline Function

**Files:**
- Modify: `src/main.c` (add new function before `init_gst`)

**Step 1: Add image pipeline initialization function**

Add this function before `init_gst`:

```c
static void init_image_pipeline(void) {
    // Initialize GStreamer if not already done
    gst_init(NULL, NULL);

    // Build simple image pipeline: filesrc ! decodebin ! videoconvert ! appsink
    char resolved_path[PATH_MAX];
    if (realpath(video_path, resolved_path) == NULL) {
        cflp_error("Failed to resolve image path '%s': %s", video_path, strerror(errno));
        exit_slapper(EXIT_FAILURE);
    }

    if (VERBOSE)
        cflp_info("Loading image: %s", resolved_path);

    // Create pipeline elements
    GstElement *filesrc = gst_element_factory_make("filesrc", "filesrc");
    GstElement *decodebin = gst_element_factory_make("decodebin", "decodebin");
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    GstElement *appsink = gst_element_factory_make("appsink", "appsink");

    if (!filesrc || !decodebin || !videoconvert || !appsink) {
        cflp_error("Failed to create image pipeline elements");
        exit_slapper(EXIT_FAILURE);
    }

    // Create pipeline
    pipeline = gst_pipeline_new("image-pipeline");
    if (!pipeline) {
        cflp_error("Failed to create image pipeline");
        exit_slapper(EXIT_FAILURE);
    }

    // Configure filesrc
    g_object_set(G_OBJECT(filesrc), "location", resolved_path, NULL);

    // Configure appsink for RGBA output
    GstCaps *caps = gst_caps_from_string("video/x-raw,format=RGBA");
    g_object_set(G_OBJECT(appsink),
                 "caps", caps,
                 "emit-signals", TRUE,
                 "sync", FALSE,  // No sync needed for single frame
                 "drop", FALSE,
                 "max-buffers", 1,
                 NULL);
    gst_caps_unref(caps);

    // Add elements to pipeline
    gst_bin_add_many(GST_BIN(pipeline), filesrc, decodebin, videoconvert, appsink, NULL);

    // Link filesrc to decodebin
    if (!gst_element_link(filesrc, decodebin)) {
        cflp_error("Failed to link filesrc to decodebin");
        exit_slapper(EXIT_FAILURE);
    }

    // Link videoconvert to appsink
    if (!gst_element_link(videoconvert, appsink)) {
        cflp_error("Failed to link videoconvert to appsink");
        exit_slapper(EXIT_FAILURE);
    }

    // Connect decodebin's dynamic pad to videoconvert
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_decodebin_pad_added), videoconvert);

    // Set up buffer probe on appsink
    GstPad *sink_pad = gst_element_get_static_pad(appsink, "sink");
    if (sink_pad) {
        gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER, buffer_probe, NULL, NULL);
        gst_object_unref(sink_pad);
    }

    // Get bus for messages
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

    // Start pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        cflp_error("Failed to start image pipeline");
        exit_slapper(EXIT_FAILURE);
    }

    // Wait for the frame to be captured (with timeout)
    int timeout_ms = 5000;
    int waited = 0;
    while (!image_frame_captured && waited < timeout_ms) {
        g_usleep(10000);  // 10ms
        waited += 10;

        // Process any pending bus messages
        GstMessage *msg = gst_bus_pop(bus);
        if (msg) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                GError *error = NULL;
                gchar *debug = NULL;
                gst_message_parse_error(msg, &error, &debug);
                cflp_error("Image decode error: %s", error->message);
                g_error_free(error);
                g_free(debug);
                gst_message_unref(msg);
                exit_slapper(EXIT_FAILURE);
            }
            gst_message_unref(msg);
        }
    }

    if (!image_frame_captured) {
        cflp_error("Timeout waiting for image frame");
        exit_slapper(EXIT_FAILURE);
    }

    // Stop pipeline (we have the frame in texture)
    gst_element_set_state(pipeline, GST_STATE_NULL);

    if (VERBOSE)
        cflp_success("Image loaded: %dx%d", video_frame_data.width, video_frame_data.height);
}
```

**Step 2: Add decodebin pad-added callback**

Add this callback function before `init_image_pipeline`:

```c
static void on_decodebin_pad_added(GstElement *decodebin, GstPad *pad, gpointer data) {
    GstElement *videoconvert = (GstElement *)data;
    GstPad *sink_pad = gst_element_get_static_pad(videoconvert, "sink");

    if (gst_pad_is_linked(sink_pad)) {
        gst_object_unref(sink_pad);
        return;
    }

    // Check if this is a video pad
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps) caps = gst_pad_query_caps(pad, NULL);

    GstStructure *str = gst_caps_get_structure(caps, 0);
    const gchar *name = gst_structure_get_name(str);

    if (g_str_has_prefix(name, "video/")) {
        GstPadLinkReturn ret = gst_pad_link(pad, sink_pad);
        if (ret != GST_PAD_LINK_OK) {
            cflp_warning("Failed to link decodebin to videoconvert");
        }
    }

    gst_caps_unref(caps);
    gst_object_unref(sink_pad);
}
```

**Step 3: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles (may have warnings about unused function until integrated)

**Step 4: Commit**

```bash
git add src/main.c
git commit -m "Add image pipeline initialization"
```

---

## Task 6: Modify Buffer Probe for Image Mode

**Files:**
- Modify: `src/main.c` (the existing buffer_probe function)

**Step 1: Find buffer_probe function**

Run: `grep -n "buffer_probe" src/main.c | head -5`

**Step 2: Add image mode handling**

In the buffer_probe function, after the frame data is captured, add:

```c
    // For image mode, mark frame as captured (single frame only)
    if (is_image_mode) {
        image_frame_captured = true;
    }
```

This should be added after the `video_frame_data.has_new_frame = TRUE;` line.

**Step 3: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors

**Step 4: Commit**

```bash
git add src/main.c
git commit -m "Add image mode handling to buffer probe"
```

---

## Task 7: Integrate Image Detection into Main Flow

**Files:**
- Modify: `src/main.c` (around line 2035 where init_gst is called)

**Step 1: Find where init_gst is called**

Run: `grep -n "init_gst" src/main.c`

**Step 2: Add image detection and pipeline selection**

Replace the `init_gst(&state);` call with:

```c
        // Detect if input is image or video
        is_image_mode = is_image_file(video_path);

        if (is_image_mode) {
            // Default to fill mode for images (unless user specified otherwise)
            if (!stretch_mode && panscan_value == 1.0f && !fill_mode) {
                fill_mode = true;
                if (VERBOSE)
                    cflp_info("Image detected, defaulting to fill mode");
            }
            init_image_pipeline();
        } else {
            init_gst(&state);
        }
```

**Step 3: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors

**Step 4: Commit**

```bash
git add src/main.c
git commit -m "Integrate image detection into initialization"
```

---

## Task 8: Modify Main Loop for Image Mode

**Files:**
- Modify: `src/main.c` (main event loop)

**Step 1: Find the main event loop**

The main loop uses poll() on wakeup_pipe and wayland display. For image mode, we don't need continuous frame updates.

**Step 2: Add image mode optimization to render**

In the render function, add early return optimization for image mode when no redraw needed:

Find the render function and add near the top (after the initial checks):

```c
    // For image mode, only render once unless redraw explicitly needed
    if (is_image_mode && !output->redraw_needed && texture_manager.initialized) {
        return;  // Image already rendered, no continuous updates needed
    }
```

**Step 3: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors

**Step 4: Commit**

```bash
git add src/main.c
git commit -m "Optimize render loop for static images"
```

---

## Task 9: Add IPC Preload Command Structure

**Files:**
- Modify: `inc/ipc.h`

**Step 1: Add preload cache structure**

Add after the ipc_command_t typedef:

```c
// Preloaded image cache entry
typedef struct preload_entry {
    char *path;
    gpointer frame_data;
    gsize frame_size;
    gint width;
    gint height;
    struct preload_entry *next;
} preload_entry_t;
```

**Step 2: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors

**Step 3: Commit**

```bash
git add inc/ipc.h
git commit -m "Add preload cache structure"
```

---

## Task 10: Add IPC Preload/Unload Handlers

**Files:**
- Modify: `src/main.c` (in execute_ipc_commands function)

**Step 1: Find execute_ipc_commands**

Run: `grep -n "execute_ipc_commands" src/main.c`

**Step 2: Add preload/unload/list command handlers**

Add these handlers in the command dispatch section (after the existing commands):

```c
        else if (strcmp(cmd_name, "preload") == 0) {
            if (!arg || strlen(arg) == 0) {
                ipc_send_response(cmd->client_fd, "ERROR: missing path argument\n");
            } else if (!is_image_file(arg)) {
                ipc_send_response(cmd->client_fd, "ERROR: not an image file\n");
            } else {
                // TODO: Implement preload cache in Phase 2
                ipc_send_response(cmd->client_fd, "OK: preload queued\n");
            }
        }
        else if (strcmp(cmd_name, "unload") == 0) {
            if (!arg || strlen(arg) == 0) {
                ipc_send_response(cmd->client_fd, "ERROR: missing path argument\n");
            } else {
                // TODO: Implement preload cache in Phase 2
                ipc_send_response(cmd->client_fd, "OK: unloaded\n");
            }
        }
        else if (strcmp(cmd_name, "list") == 0) {
            // TODO: Implement preload cache listing in Phase 2
            ipc_send_response(cmd->client_fd, "PRELOADED: (none)\n");
        }
```

**Step 3: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors

**Step 4: Commit**

```bash
git add src/main.c
git commit -m "Add IPC preload/unload/list command stubs"
```

---

## Task 11: Update Help Text

**Files:**
- Modify: `src/main.c` (help text section)

**Step 1: Find help text**

Run: `grep -n "Usage:" src/main.c`

**Step 2: Update help text with image support info**

Add to the options section:

```c
"Scaling modes (use with -o):\n"
"  fill        Fill screen maintaining aspect ratio, crop excess (default for images)\n"
"  stretch     Fill screen ignoring aspect ratio\n"
"  original    Display at native resolution\n"
"  panscan=X   Fit inside screen with scaling factor 0.0-1.0 (default for video)\n"
"\n"
"Supported formats:\n"
"  Video: MP4, MKV, WebM, AVI, MOV, and other GStreamer-supported formats\n"
"  Image: JPEG, PNG, WebP, GIF, JXL\n"
```

**Step 3: Rebuild to verify**

Run: `ninja -C build`
Expected: Compiles with no errors

**Step 4: Commit**

```bash
git add src/main.c
git commit -m "Update help text with image support"
```

---

## Task 12: Update README Documentation

**Files:**
- Modify: `README.md`

**Step 1: Add image support section**

Find the Usage section and add:

```markdown
### Static Image Wallpapers

gSlapper supports static images as well as videos:

```bash
# Display image (defaults to fill mode)
gslapper DP-1 ~/wallpaper.png

# With explicit scaling mode
gslapper -o "fill" DP-1 ~/wallpaper.jpg      # Fill screen, crop excess (default)
gslapper -o "stretch" DP-1 ~/wallpaper.webp  # Fill screen, ignore aspect
gslapper -o "panscan=1.0" DP-1 ~/wallpaper.gif  # Fit inside screen

# Supported formats
# JPEG (.jpg, .jpeg), PNG, WebP, GIF, JXL
```

**Scaling Modes:**
- `fill` - Fill screen maintaining aspect ratio, crops excess (default for images)
- `stretch` - Fill screen ignoring aspect ratio (distorts image)
- `original` - Display at native resolution (may be larger/smaller than screen)
- `panscan=X` - Fit inside screen with scale factor 0.0-1.0 (default for video)
```

**Step 2: Update IPC section**

Add to the IPC commands:

```markdown
- `preload <path>` - Preload image into memory for fast switching
- `unload <path>` - Remove image from preload cache
- `list` - List preloaded images
```

**Step 3: Commit**

```bash
git add README.md
git commit -m "Document static image support"
```

---

## Task 13: Test Image Loading

**Files:**
- None (testing only)

**Step 1: Build**

```bash
ninja -C build
```

**Step 2: Test with PNG**

```bash
# Download a test image if needed
curl -o /tmp/test.png https://via.placeholder.com/1920x1080.png

# Run gslapper with image
timeout 5 ./build/gslapper -v DP-1 /tmp/test.png
```

Expected: Image displays on screen, logs show "Image detected, defaulting to fill mode" and "Image loaded: WxH"

**Step 3: Test with JPEG**

```bash
curl -o /tmp/test.jpg https://via.placeholder.com/1920x1080.jpg
timeout 5 ./build/gslapper -v DP-1 /tmp/test.jpg
```

Expected: Similar output, image displays correctly

**Step 4: Test scaling modes**

```bash
# Test fill mode (explicit)
timeout 5 ./build/gslapper -v -o "fill" DP-1 /tmp/test.png

# Test stretch mode
timeout 5 ./build/gslapper -v -o "stretch" DP-1 /tmp/test.png

# Test fit mode
timeout 5 ./build/gslapper -v -o "panscan=1.0" DP-1 /tmp/test.png
```

Expected: Each mode behaves differently - fill crops, stretch distorts, panscan letterboxes

---

## Task 14: Test IPC with Images

**Files:**
- None (testing only)

**Step 1: Start gslapper with IPC**

```bash
./build/gslapper --ipc-socket /tmp/gslapper.sock -v DP-1 /tmp/test.png &
sleep 2
```

**Step 2: Test query command**

```bash
echo "query" | nc -U /tmp/gslapper.sock
```

Expected: `STATUS: playing /tmp/test.png`

**Step 3: Test change to different image**

```bash
echo "change /tmp/test.jpg" | nc -U /tmp/gslapper.sock
```

Expected: `OK` and image changes

**Step 4: Test preload/list commands**

```bash
echo "preload /tmp/test.png" | nc -U /tmp/gslapper.sock
echo "list" | nc -U /tmp/gslapper.sock
echo "unload /tmp/test.png" | nc -U /tmp/gslapper.sock
```

Expected: Commands respond without errors

**Step 5: Cleanup**

```bash
echo "stop" | nc -U /tmp/gslapper.sock
```

---

## Task 15: Final Commit and Push

**Files:**
- All

**Step 1: Check git status**

```bash
git status
```

Expected: Clean working directory

**Step 2: Review commit history**

```bash
git log --oneline -15
```

Expected: Clean series of commits for image support

**Step 3: Push to remote**

```bash
git push origin master
```

Expected: Successfully pushed

---

## Success Criteria

- [ ] PNG, JPEG, WebP, GIF images display correctly
- [ ] `fill` mode fills screen and crops excess (default for images)
- [ ] `stretch` mode distorts to fill
- [ ] `panscan` mode fits inside screen (with possible bars)
- [ ] `original` mode shows at native resolution
- [ ] Image detection works (extension-based)
- [ ] IPC `change` command works with images
- [ ] IPC `preload`/`unload`/`list` commands respond (stubs for Phase 2)
- [ ] Video playback still works unchanged
- [ ] Multi-monitor works with images
- [ ] README documents image support

## Deferred (Phase 2)

- JXL format support (requires gst-plugins-rs)
- Full preload cache implementation
- Transition effects (fade between images)
- Built-in slideshow mode
- AVIF format support

## Timeline

- Tasks 1-8: ~2 hours (core image support)
- Tasks 9-10: ~30 minutes (IPC stubs)
- Tasks 11-12: ~30 minutes (documentation)
- Tasks 13-14: ~30 minutes (testing)
- Task 15: ~10 minutes (push)

**Total: ~3.5 hours**
