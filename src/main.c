/*
 * slapper - GStreamer-based wallpaper player for Wayland
 * 
 * This is a clean implementation that replaces mpvpaper's libmpv backend
 * with GStreamer to solve memory leak issues on Wayland/NVIDIA systems.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <mcheck.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include <wayland-client.h>
#include <wayland-egl.h>

#include <glad/glad.h>
#include <glad/glad_egl.h>

// GStreamer includes
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/videooverlay.h>
#include <gst/gl/gl.h>

#include <cflogprinter.h>

typedef unsigned int uint;

struct wl_state {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct wl_list outputs; // struct display_output::link
    char *monitor; // User selected output
    int surface_layer;
};

struct display_output {
    uint32_t wl_name;
    struct wl_output *wl_output;
    char *name;
    char *identifier;

    struct wl_state *state;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wl_egl_window *egl_window;
    EGLSurface *egl_surface;

    uint32_t width, height;
    uint32_t scale;

    struct wl_list link;

    struct wl_callback *frame_callback;
    bool redraw_needed;
};

// GStreamer elements
static GstElement *pipeline;
static GstBus *bus;
static char *allocated_uri = NULL;  // Track URI allocation for cleanup

// Track if we're using waylandsink for direct rendering
static bool using_waylandsink = false;

// Video texture information
static GLuint video_texture = 0;
static GLuint shader_program = 0;
static GLuint vao = 0, vbo = 0;
static pthread_mutex_t video_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    gint last_width, last_height; // Track dimension changes for efficient updates
} video_frame_data = {0};

static struct wl_state *global_state = NULL;

// Frame rate limiting for GPU optimization
static struct timespec last_render_time = {0};
static long long target_frame_time_ns = 33333333; // Default ~30 FPS (1/30 * 1e9)
static int frame_rate_cap = 30; // Default 30 FPS
static int frames_skipped = 0; // Track skipped frames for adaptive skipping
static char *video_path;
static char *gst_options = "";

static EGLConfig egl_config;
static EGLDisplay *egl_display;
static EGLContext *egl_context;

static int wakeup_pipe[2];

// State management
static struct {
    char **pauselist;
    char **stoplist;

    int argc;
    char **argv_copy;
    char *save_info;

    bool auto_pause;
    bool auto_stop;

    int is_paused;
    bool frame_ready;
    bool stop_render_loop;

} halt_info = {NULL, NULL, 0, NULL, NULL, 0, 0, 0, 0, 0};

static pthread_t threads[5] = {0};

static uint SLIDESHOW_TIME = 0;
static bool SHOW_OUTPUTS = false;
static int VERBOSE = 0;

// Forward declarations
static void exit_cleanup();
static void exit_slapper(int reason);
static void handle_signal(int signum);
static void render(struct display_output *output);
static void stop_slapper();
static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data);
static void *handle_gst_events(void *);
static void init_texture_manager();
static void cleanup_texture_manager();
static GLuint get_texture_for_dimensions(int width, int height);
static GLuint create_shader_program();

// Cleanup function
static void exit_cleanup() {

    // Give GStreamer a chance to finish
    halt_info.stop_render_loop = 1;
    for (int trys=10; halt_info.stop_render_loop && trys > 0; trys--) {
        usleep(10000);
    }
    // If render loop failed to stop it's self
    if (halt_info.stop_render_loop && VERBOSE)
        cflp_warning("Failed to quit GStreamer");

    // Cancel all threads
    for (uint i=0; threads[i] != 0; i++) {
        if (pthread_self() != threads[i])
            pthread_cancel(threads[i]);
    }

    // Clean up texture manager
    cleanup_texture_manager();
    
    if (pipeline) {
        // More graceful GStreamer shutdown sequence
        if (VERBOSE)
            cflp_info("Starting graceful GStreamer shutdown...");
            
        // Remove bus watch to prevent callbacks during shutdown
        if (bus) {
            gst_bus_remove_watch(bus);
            gst_object_unref(bus);
            bus = NULL;
        }
        
        // Gradual state transition for clean shutdown
        gst_element_set_state(pipeline, GST_STATE_PAUSED);
        usleep(100000); // Give it time to pause
        
        gst_element_set_state(pipeline, GST_STATE_READY);
        usleep(100000); // Give it time to clean up resources
        
        gst_element_set_state(pipeline, GST_STATE_NULL);
        usleep(200000); // Give it time to fully shut down
        
        // Now unref the pipeline
        gst_object_unref(pipeline);
        pipeline = NULL;
        
        if (VERBOSE)
            cflp_info("GStreamer shutdown completed");
    }
    
    // Clean up allocated URI
    if (allocated_uri) {
        free(allocated_uri);
        allocated_uri = NULL;
    }

    if (egl_context)
        eglDestroyContext(egl_display, egl_context);
    
    // Free allocated memory
    if (gst_options && strlen(gst_options) > 0)
        free((void*)gst_options);
    
    if (video_path)
        free(video_path);
    
        
    if (halt_info.save_info)
        free(halt_info.save_info);
    
    // Free argv copy
    if (halt_info.argv_copy) {
        for (int i = 0; i < halt_info.argc; i++) {
            if (halt_info.argv_copy[i])
                free(halt_info.argv_copy[i]);
        }
        free(halt_info.argv_copy);
    }
    
    // Free watch lists
    if (halt_info.pauselist) {
        for (int i = 0; halt_info.pauselist[i] != NULL; i++) {
            free(halt_info.pauselist[i]);
        }
        free(halt_info.pauselist);
    }
    
    if (halt_info.stoplist) {
        for (int i = 0; halt_info.stoplist[i] != NULL; i++) {
            free(halt_info.stoplist[i]);
        }
        free(halt_info.stoplist);
    }
    
    // Clean up video frame data
    pthread_mutex_lock(&video_mutex);
    if (video_frame_data.data) {
        g_free(video_frame_data.data);
        video_frame_data.data = NULL;
    }
    pthread_mutex_unlock(&video_mutex);
    
    // Clean up OpenGL resources
    if (video_texture != 0) {
        glDeleteTextures(1, &video_texture);
        video_texture = 0;
    }
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (shader_program != 0) {
        glDeleteProgram(shader_program);
        shader_program = 0;
    }
}

// Initialize smart texture manager
static void init_texture_manager() {
    glGenTextures(1, &texture_manager.texture);
    texture_manager.current_width = 0;
    texture_manager.current_height = 0;
    texture_manager.initialized = FALSE;
    
    if (VERBOSE)
        cflp_info("Initialized smart texture manager");
}

// Clean up texture manager
static void cleanup_texture_manager() {
    if (texture_manager.initialized) {
        glDeleteTextures(1, &texture_manager.texture);
        texture_manager.texture = 0;
        texture_manager.initialized = FALSE;
        if (VERBOSE)
            cflp_info("Cleaned up texture manager");
    }
}

// Get texture for current dimensions with smart allocation
static GLuint get_texture_for_dimensions(int width, int height) {
    if (!texture_manager.initialized) {
        // First-time initialization
        glBindTexture(GL_TEXTURE_2D, texture_manager.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        texture_manager.current_width = width;
        texture_manager.current_height = height;
        texture_manager.initialized = TRUE;
        
        if (VERBOSE)
            cflp_info("Texture initialized for dimensions: %dx%d", width, height);
        
        return texture_manager.texture;
    }
    
    // Check if we need to reallocate
    if (texture_manager.current_width != width || texture_manager.current_height != height) {
        glBindTexture(GL_TEXTURE_2D, texture_manager.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        texture_manager.current_width = width;
        texture_manager.current_height = height;
        
        if (VERBOSE)
            cflp_info("Texture reallocated for new dimensions: %dx%d", width, height);
    }
    
    return texture_manager.texture;
}

static void exit_slapper(int reason) {
    if (VERBOSE)
        cflp_info("Exiting slapper");
    exit_cleanup();
    exit(reason);
}

static void *exit_by_pthread(void *_) {
    exit_slapper(EXIT_SUCCESS);
    pthread_exit(NULL);
}

static void handle_signal(int signum) {
    (void)signum;
    // Separate thread to avoid crash
    pthread_t thread;
    pthread_create(&thread, NULL, exit_by_pthread, NULL);
}

const static struct wl_callback_listener wl_surface_frame_listener;

static GLuint create_shader_program() {
    const char *vertex_shader_source = 
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "}\0";
    
    const char *fragment_shader_source = 
        "#version 330 core\n"
        "in vec2 TexCoord;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D ourTexture;\n"
        "void main() {\n"
        "    FragColor = texture(ourTexture, TexCoord);\n"
        "}\0";
    
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    
    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
        cflp_error("Vertex shader compilation failed: %s", info_log);
        return 0;
    }
    
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
        cflp_error("Fragment shader compilation failed: %s", info_log);
        glDeleteShader(vertex_shader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        cflp_error("Shader program linking failed: %s", info_log);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(program);
        return 0;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    return program;
}

static void render(struct display_output *output) {
    // If using waylandsink, we don't need EGL rendering - waylandsink handles it
    if (using_waylandsink) {
        // Just schedule the next frame callback
        output->frame_callback = wl_surface_frame(output->surface);
        wl_callback_add_listener(output->frame_callback, &wl_surface_frame_listener, output);
        output->redraw_needed = false;
        return;
    }

    // Make sure we have a valid context
    if (!eglMakeCurrent(egl_display, output->egl_surface, output->egl_surface, egl_context)) {
        cflp_error("Failed to make output surface current");
        return;
    }

    glViewport(0, 0, output->width * output->scale, output->height * output->scale);
    
    // Clear with black background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Frame rate limiting for GPU optimization
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    long long time_since_last_render = (current_time.tv_sec - last_render_time.tv_sec) * 1000000000LL + 
                                       (current_time.tv_nsec - last_render_time.tv_nsec);
    
    // Adaptive frame skipping for better performance under load
    gboolean should_render = FALSE;
    pthread_mutex_lock(&video_mutex);
    
    if (video_frame_data.has_new_frame) {
        if (VERBOSE)
            cflp_info("FOUND NEW FRAME - will render");
        should_render = TRUE; // Always render if we have a new frame
        frames_skipped = 0; // Reset skip counter when we render
    } else {
        if (VERBOSE)
            cflp_info("No new frame found");
    }
    
    if (should_render) {
        if (VERBOSE == 2) {
            static int process_count = 0;
            process_count++;
            if (process_count % 100 == 0)  // Log every 100th frame
                cflp_info("Processing new video frame: %dx%d, data=%p (frame %d)", 
                         video_frame_data.width, video_frame_data.height, video_frame_data.data, process_count);
        }
        
        // Check OpenGL error before texture update
        GLenum err_before = glGetError();
        if (err_before != GL_NO_ERROR && VERBOSE)
            cflp_info("OpenGL error before texture update: 0x%x", err_before);
        
        // Update texture data with efficient sub-image update
        GLuint current_texture = get_texture_for_dimensions(video_frame_data.width, video_frame_data.height);
        glBindTexture(GL_TEXTURE_2D, current_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, video_frame_data.width, video_frame_data.height, 
                        GL_RGBA, GL_UNSIGNED_BYTE, video_frame_data.data);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        // Check OpenGL error after texture update
        GLenum err_after = glGetError();
        if (err_after != GL_NO_ERROR)
            cflp_info("OpenGL error after texture update: 0x%x", err_after);
        
        // Free frame data and reset flag
        g_free(video_frame_data.data);
        video_frame_data.data = NULL;
        video_frame_data.has_new_frame = FALSE;
        
        // Update last render time
        last_render_time = current_time;
        
        if (VERBOSE == 2) {
            static int texture_count = 0;
            texture_count++;
            if (texture_count % 100 == 0)  // Log every 100th frame
                cflp_info("Updated video texture %u with %dx%d frame (update %d)", texture_manager.texture, video_frame_data.width, video_frame_data.height, texture_count);
        }
    } else if (texture_manager.initialized && texture_manager.texture != 0 && time_since_last_render >= target_frame_time_ns) {
        // No new frame, but we should still render the existing texture at target frame rate
        if (VERBOSE == 2) {
            static int render_only_count = 0;
            render_only_count++;
            if (render_only_count % 100 == 0)
                cflp_info("Rendering existing texture (render-only %d)", render_only_count);
        }
        // Update last render time
        last_render_time = current_time;
    } else {
        if (VERBOSE == 2)
            cflp_info("No new video frame available (frame rate limited)");
    }
    
    // Render video texture if available
    if (texture_manager.initialized && texture_manager.texture != 0) {
        if (VERBOSE == 2) {
            static int render_count = 0;
            render_count++;
            if (render_count % 100 == 0)  // Log every 100th frame
                cflp_info("Rendering video texture %u (render %d)", texture_manager.texture, render_count);
        }
        
        // Don't clear again - we want to see the video texture
        
        // Create shader program if needed
        if (shader_program == 0) {
            shader_program = create_shader_program();
            if (shader_program == 0) {
                cflp_error("Failed to create shader program");
                return;
            }
            if (VERBOSE)  // Keep this as it's a one-time message
                cflp_info("Created shader program %u", shader_program);
        }
        
        // Create VAO and VBO if needed
        if (vao == 0) {
            float vertices[] = {
                // Position (x, y)  // Texture coords (s, t)
                -1.0f, -1.0f,      0.0f, 1.0f,  // bottom-left
                 1.0f, -1.0f,      1.0f, 1.0f,  // bottom-right
                 1.0f,  1.0f,      1.0f, 0.0f,  // top-right
                -1.0f,  1.0f,      0.0f, 0.0f   // top-left
            };
            
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            
            // Position attribute
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            
            // Texture coordinate attribute
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glEnableVertexAttribArray(1);
            
            glBindVertexArray(0);
            
            if (VERBOSE)  // Keep this as it's a one-time message
                cflp_info("Created VAO %u and VBO %u", vao, vbo);
        }
        
        // Use shader program
        glUseProgram(shader_program);
        
        // Bind texture from smart manager
        glActiveTexture(GL_TEXTURE0);
        GLuint render_texture = get_texture_for_dimensions(video_frame_data.width, video_frame_data.height);
        glBindTexture(GL_TEXTURE_2D, render_texture);
        glUniform1i(glGetUniformLocation(shader_program, "ourTexture"), 0);
        
        // Draw the quad
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
        
        // Clean up
        glUseProgram(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        if (VERBOSE == 2) {
            static int complete_count = 0;
            complete_count++;
            if (complete_count % 100 == 0)  // Log every 100th frame
                cflp_info("Shader-based rendering complete (render %d)", complete_count);
        }
    } else {
        if (VERBOSE)
            cflp_info("No video texture available for rendering");
    }
    pthread_mutex_unlock(&video_mutex);

    // Callback new frame
    output->frame_callback = wl_surface_frame(output->surface);
    wl_callback_add_listener(output->frame_callback, &wl_surface_frame_listener, output);
    output->redraw_needed = false;

    // Display frame
    if (!eglSwapBuffers(egl_display, output->egl_surface))
        cflp_error("Failed to swap egl buffers");
}

static void frame_handle_done(void *data, struct wl_callback *callback, uint32_t frame_time) {
    (void)frame_time;
    struct display_output *output = data;
    wl_callback_destroy(callback);

    // Display is ready for new frame
    output->frame_callback = NULL;

    // Reset deadman switch timer
    halt_info.frame_ready = 1;

    // Render next frame
    if (output->redraw_needed) {
        if (VERBOSE == 2)
            cflp_info("%s is ready for rendering", output->name);
        render(output);
    }
}

const static struct wl_callback_listener wl_surface_frame_listener = {
    .done = frame_handle_done,
};

static void stop_slapper() {

    // Save video position to arg -Z
    char save_info[64] = "0 0"; // Default position
    
    if (pipeline) {
        gint64 position = 0;
        // Query the current position from GStreamer
        if (gst_element_query_position(pipeline, GST_FORMAT_TIME, &position)) {
            // Convert nanoseconds to seconds for easier handling
            gint64 seconds = position / GST_SECOND;
            gint64 playlist_pos = 0; // For now, we don't track playlist position
            
            // Format as "seconds playlist_position"
            snprintf(save_info, sizeof(save_info), "%" G_GINT64_FORMAT " %" G_GINT64_FORMAT, seconds, playlist_pos);
            
            if (VERBOSE) {
                cflp_info("Saving position: %s seconds, playlist pos: %" G_GINT64_FORMAT, 
                         save_info, playlist_pos);
            }
        } else {
            cflp_warning("Failed to query current position");
        }
    }

    char **new_argv = calloc(halt_info.argc + 3, sizeof(char *)); // Plus 3 for adding in -Z
    if (!new_argv) {
        cflp_error("Failed to allocate new argv");
        exit(EXIT_FAILURE);
    }

    uint i = 0;
    for (i=0; i < halt_info.argc; i++) {
        new_argv[i] = strdup(halt_info.argv_copy[i]);
    }
    new_argv[i] = strdup("-Z");
    new_argv[i+1] = strdup(save_info);
    new_argv[i+2] = NULL;

    // Get the "real" cwd
    char exe_dir[1024];
    int cut_point = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir));
    for (uint i=cut_point; i > 1; i--) {
        if (exe_dir[i] == '/') {
            exe_dir[i+1] = '\0';
            break;
        }
    }

    exit_cleanup();

    // Start holder script
    execv(strcat(exe_dir, "slapper-holder"), new_argv);

    cflp_error("Failed to stop clappie");
    exit(EXIT_FAILURE);
}

// Thread helpers
static void pthread_sleep(uint time) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    sleep(time);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
}
static void pthread_usleep(uint time) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    usleep(time);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
}

// Process monitoring
static char *check_watch_list(char **list) {

    char pid_name[512] = {0};

    for (uint i=0; list[i] != NULL; i++) {
        snprintf(pid_name, sizeof(pid_name), "pidof %s > /dev/null", list[i]);

        // Stop if program is open
        if (!system(pid_name))
            return list[i];
    }
    return NULL;
}

static void *monitor_pauselist(void *_) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    bool list_paused = 0;

    while (halt_info.pauselist) {

        char *app = check_watch_list(halt_info.pauselist);
        if (app && !list_paused && !halt_info.is_paused) {
            if (VERBOSE)
                cflp_info("Pausing for %s", app);
            // For GStreamer, we pause the pipeline
            if (pipeline)
                gst_element_set_state(pipeline, GST_STATE_PAUSED);
            list_paused = 1;
            halt_info.is_paused += 1;
        } else if (!app && list_paused) {
            list_paused = 0;
            if (halt_info.is_paused)
                halt_info.is_paused -= 1;
        }

        pthread_sleep(1);
    }
    pthread_exit(NULL);
}

static void *monitor_stoplist(void *_) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    while (halt_info.stoplist) {

        char *app = check_watch_list(halt_info.stoplist);
        if (app) {
            if (VERBOSE)
                cflp_info("Stopping for %s", app);
            stop_slapper();
        }

        pthread_sleep(1);
    }
    pthread_exit(NULL);
}

static void *handle_auto_pause(void *_) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    while (halt_info.auto_pause) {

        // Set deadman switch timer
        halt_info.frame_ready = 0;
        pthread_sleep(2);
        if (!halt_info.frame_ready && !halt_info.is_paused) {
            if (VERBOSE)
                cflp_info("Pausing because clappie is hidden");
            // For GStreamer, we pause the pipeline
            if (pipeline)
                gst_element_set_state(pipeline, GST_STATE_PAUSED);
            halt_info.is_paused += 1;

            while (!halt_info.frame_ready) {
                pthread_usleep(10000);
            }
            if (halt_info.is_paused)
                halt_info.is_paused -= 1;
        }
    }
    pthread_exit(NULL);
}

static void *handle_auto_stop(void *_) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    while (halt_info.auto_stop) {

        // Set deadman switch timer
        halt_info.frame_ready = 0;
        pthread_sleep(2);
        if (!halt_info.frame_ready) {
            if (VERBOSE)
                cflp_info("Stopping because clappie is hidden");
            stop_slapper();
        }
    }
    pthread_exit(NULL);
}

// GStreamer event handling
static GstPadProbeReturn buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    
    if (buffer) {
        if (VERBOSE == 2)
            cflp_info("appsink callback triggered with buffer");
        pthread_mutex_lock(&video_mutex);
        
        // Get the caps to understand the format
        GstCaps *caps = gst_pad_get_current_caps(pad);
        if (caps) {
            GstStructure *structure = gst_caps_get_structure(caps, 0);
            const gchar *format = gst_structure_get_string(structure, "format");
            
            if (VERBOSE == 2)
                cflp_info("Frame received with format: %s", format ? format : "NULL");
            
            if (format && strcmp(format, "RGBA") == 0) {
                // Extract video data and queue for texture update
                GstMapInfo map;
                if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                    gint width, height;
                    gst_structure_get_int(structure, "width", &width);
                    gst_structure_get_int(structure, "height", &height);
                    
                    // Free old frame data if any
                    if (video_frame_data.data) {
                        g_free(video_frame_data.data);
                    }
                    
                    // Copy frame data for texture update in render thread
                    video_frame_data.data = g_memdup2(map.data, map.size);
                    video_frame_data.size = map.size;
                    video_frame_data.width = width;
                    video_frame_data.height = height;
                    video_frame_data.has_new_frame = TRUE;
                    
                    if (VERBOSE == 2) {
                        static int frame_count = 0;
                        frame_count++;
                        if (frame_count % 100 == 0)  // Log every 100th frame
                            cflp_info("Captured %dx%d RGBA frame for texture update (frame %d)", width, height, frame_count);
                    }
                    
                    // Trigger render via wakeup pipe to ensure thread safety
                    if (write(wakeup_pipe[1], "f", 1) == -1) {
                        // If pipe write fails, try to trigger directly
                        if (VERBOSE)
                            cflp_warning("Failed to write to wakeup pipe");
                    }
                    
                    gst_buffer_unmap(buffer, &map);
                }
            } else {
                if (VERBOSE == 2)
                    cflp_info("Frame format is %s, not RGBA", format ? format : "unknown");
            }
            gst_caps_unref(caps);
        }
        
        pthread_mutex_unlock(&video_mutex);
    }
    
    return GST_PAD_PROBE_OK;
}

static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err = NULL;
            gchar *debug_info = NULL;
            gst_message_parse_error(msg, &err, &debug_info);
            cflp_error("GStreamer error: %s", err->message);
            
            // Print additional debug information if available
            if (debug_info) {
                cflp_error("Debug info: %s", debug_info);
            }
            
            g_error_free(err);
            g_free(debug_info);
            exit_slapper(EXIT_FAILURE);
            break;
        }
        case GST_MESSAGE_EOS:
            if (VERBOSE)
                cflp_info("End of stream reached - fallback loop method");
            // Fallback: if we get EOS instead of SEGMENT_DONE, still loop
            if (pipeline) {
                if (!gst_element_seek_simple(pipeline, GST_FORMAT_TIME, 
                                           GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT, 0)) {
                    cflp_warning("EOS fallback seek failed");
                }
            }
            break;
        case GST_MESSAGE_SEGMENT_DONE:
            if (VERBOSE)
                cflp_info("Segment done - seamless loop restart");
            // CHANGED 2025-09-07 - Use segment-based seamless looping per GStreamer best practices
            // Problem: EOS-based seeking causes playback gaps, segment seeks provide seamless transitions
            if (pipeline) {
                if (!gst_element_seek_simple(pipeline, GST_FORMAT_TIME, 
                                           GST_SEEK_FLAG_SEGMENT, 0)) {
                    cflp_warning("Segment seek failed for seamless loop");
                    // Fallback to regular seek
                    gst_element_seek_simple(pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, 0);
                }
            }
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                if (new_state == GST_STATE_PLAYING) {
                    if (VERBOSE)
                        cflp_success("GStreamer pipeline is playing");
                    
                    // CHANGED 2025-09-07 - Initialize segment-based looping when pipeline starts
                    // Problem: Need initial segment seek to enable SEGMENT_DONE messages instead of EOS
                    static bool segment_initialized = false;
                    if (!segment_initialized) {
                        if (VERBOSE)
                            cflp_info("Setting up seamless segment-based looping");
                        
                        // Perform initial seek to enable segment looping
                        if (!gst_element_seek(pipeline, 1.0, GST_FORMAT_TIME, 
                                            GST_SEEK_FLAG_SEGMENT,
                                            GST_SEEK_TYPE_SET, 0,
                                            GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
                            cflp_warning("Failed to initialize segment looping - falling back to EOS method");
                        } else {
                            if (VERBOSE)
                                cflp_success("Segment looping initialized successfully");
                        }
                        segment_initialized = true;
                    }
                }
            }
            break;
        }
        default:
            break;
    }
    return TRUE;
}

static void *handle_gst_events(void *_) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    time_t start_time = time(NULL);

    while (true) {
        if (SLIDESHOW_TIME) {
            if ((time(NULL) - start_time) >= SLIDESHOW_TIME) {
                // For slideshow mode, we would need to implement playlist handling
                // This is a placeholder
                cflp_info("Slideshow next (not implemented)");
                start_time = time(NULL);
            }
        }

        // Handle GStreamer messages
        if (bus) {
            GstMessage *msg = gst_bus_timed_pop_filtered(bus, 10000, GST_MESSAGE_ANY);
            
            if (msg) {
                bus_callback(bus, msg, NULL);
                gst_message_unref(msg);
            }
        }

        pthread_usleep(10000);
    }

    pthread_exit(NULL);
}

static void init_threads() {
    uint id = 0;

    pthread_create(&threads[id], NULL, handle_gst_events, NULL);
    id++;

    // Thread for monitoring if clappie is hidden
    if (halt_info.auto_pause) {
        pthread_create(&threads[id], NULL, handle_auto_pause, NULL);
        id++;
    } else if (halt_info.auto_stop) {
        pthread_create(&threads[id], NULL, handle_auto_stop, NULL);
        id++;
    }

    // Threads for monitoring watch lists
    if (halt_info.pauselist) {
        pthread_create(&threads[id], NULL, monitor_pauselist, NULL);
        id++;
    }
    if (halt_info.stoplist) {
        pthread_create(&threads[id], NULL, monitor_stoplist, NULL);
        id++;
    }
}

// GStreamer option handling
static void set_frame_rate_cap(int fps) {
    switch (fps) {
        case 30:
            target_frame_time_ns = 33333333; // 1/30 * 1e9
            frame_rate_cap = 30;
            break;
        case 60:
            target_frame_time_ns = 16666667; // 1/60 * 1e9
            frame_rate_cap = 60;
            break;
        case 100:
            target_frame_time_ns = 10000000; // 1/100 * 1e9
            frame_rate_cap = 100;
            break;
        default:
            cflp_warning("Invalid frame rate cap %d, using 30 FPS", fps);
            target_frame_time_ns = 33333333;
            frame_rate_cap = 30;
            break;
    }
    
    if (VERBOSE)
        cflp_info("Frame rate cap set to %d FPS", frame_rate_cap);
}

static void apply_gst_options() {
    if (VERBOSE)
        cflp_info("Applying GStreamer options: %s", gst_options);
    
    // Parse gst_options and apply them to the pipeline
    if (strstr(gst_options, "no-audio") != NULL || strstr(gst_options, "mute") != NULL) {
        // Disable audio by setting flags
        gint flags = 0x00000001; // GST_PLAY_FLAG_VIDEO only
        g_object_set(G_OBJECT(pipeline), "flags", flags, NULL);
    } else {
        // Enable both video and audio
        gint flags = 0x00000003; // GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO
        g_object_set(G_OBJECT(pipeline), "flags", flags, NULL);
    }
    
    // Handle loop options
    if (strstr(gst_options, "loop") != NULL || SLIDESHOW_TIME != 0) {
        // Loop is handled by seeking to beginning on EOS
        if (VERBOSE)
            cflp_info("Looping enabled");
    }
}

// GStreamer initialization
static void init_gst(const struct wl_state *state) {
    // Initialize GStreamer
    gst_init(NULL, NULL);

    // Create playbin element (higher-level player)
    pipeline = gst_element_factory_make("playbin", "playbin");
    if (!pipeline) {
        cflp_error("Failed to create playbin element");
        exit_slapper(EXIT_FAILURE);
    }

    // Set the URI - convert local file path to URI if needed
    char *uri = video_path;
    if (strstr(video_path, "://") == NULL) {
        // Local file, convert to file:// URI
        uri = malloc(strlen(video_path) + 8);
        sprintf(uri, "file://%s", video_path);
        allocated_uri = uri;  // Track for cleanup
    }
    g_object_set(G_OBJECT(pipeline), "uri", uri, NULL);

    // Set flags for video + audio playback by default
    gint flags = 0x00000003; // GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO
    g_object_set(G_OBJECT(pipeline), "flags", flags, NULL);

    // Use appsink to get video frames for manual rendering
    GstElement *app_sink = gst_element_factory_make("appsink", "app-sink");
    if (app_sink) {
        // Configure appsink to output RGBA frames
        GstCaps *caps = gst_caps_from_string("video/x-raw,format=RGBA");
        g_object_set(G_OBJECT(app_sink), 
                     "caps", caps,
                     "emit-signals", TRUE,
                     "sync", TRUE,
                     "drop", TRUE,
                     "max-buffers", 1,  // Limit to 1 buffer to prevent accumulation
                     NULL);
        gst_caps_unref(caps);
        
        g_object_set(G_OBJECT(pipeline), "video-sink", app_sink, NULL);
        
        using_waylandsink = false;  // We'll handle rendering manually
        if (VERBOSE)
            cflp_info("Using appsink for manual texture rendering");
    } else {
        // Fallback to autovideosink
        GstElement *auto_sink = gst_element_factory_make("autovideosink", "auto-sink");
        if (auto_sink) {
            g_object_set(G_OBJECT(pipeline), "video-sink", auto_sink, NULL);
            if (VERBOSE)
                cflp_info("Using autovideosink for video output");
        } else {
            // Last resort fakesink
            GstElement *fake_sink = gst_element_factory_make("fakesink", "fake-sink");
            if (fake_sink) {
                g_object_set(G_OBJECT(fake_sink), "sync", TRUE, NULL);
                g_object_set(G_OBJECT(pipeline), "video-sink", fake_sink, NULL);
                if (VERBOSE)
                    cflp_info("Using fakesink for video output");
            }
        }
    }

    // Get bus for messages
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_watch(bus, bus_callback, NULL);

    // Apply GStreamer options
    apply_gst_options();

    // Set up buffer probe to capture video frames from appsink
    GstElement *video_sink = NULL;
    g_object_get(G_OBJECT(pipeline), "video-sink", &video_sink, NULL);
    if (video_sink) {
        // Get the sink pad to add the probe
        GstPad *sink_pad = gst_element_get_static_pad(video_sink, "sink");
        if (sink_pad) {
            gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER, buffer_probe, NULL, NULL);
            gst_object_unref(sink_pad);
        }
        gst_object_unref(video_sink);
    }

    // Start playing
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    // Handle the state change result properly
    if (ret == GST_STATE_CHANGE_FAILURE) {
        // This is a real error - the state change failed
        cflp_error("Failed to set pipeline to playing state");
        
        // Get more detailed error information
        GError *error = NULL;
        gchar *debug = NULL;
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR);
        
        if (msg) {
            gst_message_parse_error(msg, &error, &debug);
            cflp_error("GStreamer error: %s", error->message);
            if (debug) {
                cflp_error("Debug info: %s", debug);
            }
            g_error_free(error);
            g_free(debug);
            gst_message_unref(msg);
        }
        
        exit_slapper(EXIT_FAILURE);
    } else {
        // Either GST_STATE_CHANGE_ASYNC (normal) or GST_STATE_CHANGE_SUCCESS (immediate)
        if (VERBOSE) {
            if (ret == GST_STATE_CHANGE_ASYNC) {
                cflp_info("Pipeline state change in progress (async)");
            } else if (ret == GST_STATE_CHANGE_SUCCESS) {
                cflp_info("Pipeline started immediately");
            }
        }
        
        // Wait a bit to see if the pipeline actually starts
        if (ret == GST_STATE_CHANGE_ASYNC) {
            GstStateChangeReturn wait_ret = gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND);
            if (wait_ret == GST_STATE_CHANGE_FAILURE) {
                cflp_error("Pipeline failed to reach playing state");
                exit_slapper(EXIT_FAILURE);
            }
        }
        
        // Restore position if we have saved info
        if (halt_info.save_info && strlen(halt_info.save_info) > 0) {
            // Parse the saved position (format: "seconds playlist_position")
            gint64 saved_seconds = 0;
            gint64 playlist_pos = 0;
            
            if (sscanf(halt_info.save_info, "%" G_GINT64_FORMAT " %" G_GINT64_FORMAT, &saved_seconds, &playlist_pos) == 2) {
                if (saved_seconds > 0) {
                    gint64 seek_pos = saved_seconds * GST_SECOND;
                    if (gst_element_seek(pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                                       GST_SEEK_TYPE_SET, seek_pos,
                                       GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
                        if (VERBOSE) {
                            cflp_info("Restored position to %" G_GINT64_FORMAT " seconds", saved_seconds);
                        }
                    } else {
                        cflp_warning("Failed to restore position to %" G_GINT64_FORMAT " seconds", saved_seconds);
                    }
                }
            } else {
                cflp_warning("Failed to parse saved position info: %s", halt_info.save_info);
            }
        }
    }

    if (VERBOSE)
        cflp_info("Loaded %s", video_path);
}

// EGL initialization (copied from mpvpaper)
static void init_egl(struct wl_state *state) {
    egl_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, state->display, NULL);
    if (egl_display == EGL_NO_DISPLAY) {
        cflp_error("Failed to get EGL display");
        exit_slapper(EXIT_FAILURE);
    }
    if (!eglInitialize(egl_display, NULL, NULL)) {
        cflp_error("Failed to initialize EGL");
        exit_slapper(EXIT_FAILURE);
    }

    eglBindAPI(EGL_OPENGL_API);
    const EGLint win_attrib[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLint num_config;
    if (!eglChooseConfig(egl_display, win_attrib, &egl_config, 1, &num_config)) {
        cflp_error("Failed to set EGL frame buffer config");
        exit_slapper(EXIT_FAILURE);
    }

    // Check for OpenGL compatibility for creating egl context
    // Try compatibility contexts first for immediate mode support
    static const struct { int major, minor; } gl_versions[] = {
        {3, 3}, {3, 2}, {3, 1}, {3, 0},
        {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0},
        {0, 0}
    };
    egl_context = NULL;
    for (uint i=0; gl_versions[i].major > 0; i++) {
        const EGLint ctx_attrib[] = {
            EGL_CONTEXT_MAJOR_VERSION, gl_versions[i].major,
            EGL_CONTEXT_MINOR_VERSION, gl_versions[i].minor,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
            EGL_NONE
        };
        egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, ctx_attrib);
        if (egl_context) {
            if (VERBOSE)
                cflp_info("OpenGL %i.%i Compatibility EGL context created", gl_versions[i].major, gl_versions[i].minor);
            break;
        }
    }
    
    // Fallback to core context if compatibility fails
    if (!egl_context) {
        for (uint i=0; gl_versions[i].major > 0; i++) {
            const EGLint ctx_attrib[] = {
                EGL_CONTEXT_MAJOR_VERSION, gl_versions[i].major,
                EGL_CONTEXT_MINOR_VERSION, gl_versions[i].minor,
                EGL_NONE
            };
            egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, ctx_attrib);
            if (egl_context) {
                if (VERBOSE)
                    cflp_info("OpenGL %i.%i Core EGL context created", gl_versions[i].major, gl_versions[i].minor);
                break;
            }
        }
    }
    if (!egl_context) {
        cflp_error("Failed to create EGL context");
        exit_slapper(EXIT_FAILURE);
    }

    if (!eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context)) {
        cflp_error("Failed to make context current");
        exit_slapper(EXIT_FAILURE);
    }

    if (!gladLoadGLLoader((GLADloadproc)eglGetProcAddress)) {
        cflp_error("Failed to load OpenGL");
        exit_slapper(EXIT_FAILURE);
    }
    
    // Initialize smart texture manager
    init_texture_manager();
}

// Wayland output management (copied from mpvpaper)
static void destroy_display_output(struct display_output *output) {
    if (!output)
        return;
    wl_list_remove(&output->link);
    if (output->layer_surface != NULL)
        zwlr_layer_surface_v1_destroy(output->layer_surface);
    if (output->surface != NULL)
        wl_surface_destroy(output->surface);
    if (output->egl_surface)
        eglDestroySurface(egl_display, output->egl_surface);
    if (output->egl_window)
        wl_egl_window_destroy(output->egl_window);
    wl_output_destroy(output->wl_output);

    free(output->name);
    free(output->identifier);
    free(output);
}

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width,
        uint32_t height) {

    struct display_output *output = data;
    output->width = width;
    output->height = height;
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    wl_surface_set_buffer_scale(output->surface, output->scale);

    if (!output->egl_window) {
        output->egl_window = wl_egl_window_create(output->surface, output->width * output->scale,
                output->height * output->scale);
        output->egl_surface = eglCreatePlatformWindowSurface(egl_display, egl_config, output->egl_window, NULL);
        if (!output->egl_surface) {
            cflp_error("Failed to create EGL surface for %s", output->name);
            destroy_display_output(output);
            return;
        }

        if (!eglMakeCurrent(egl_display, output->egl_surface, output->egl_surface, egl_context))
            cflp_error("Failed to make output surface current");
        eglSwapInterval(egl_display, 0);

        // After making EGL_NO_SURFACE current to a context
        // Only with the Nvidia Pro drivers will set the draw buffer state to GL_NONE
        // So we are going to force GL_BACK just like Mesa's EGL implementation
        glDrawBuffer(GL_BACK);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        // Start render loop
        render(output);
    } else {
        wl_egl_window_resize(output->egl_window, output->width * output->scale, output->height * output->scale, 0, 0);
    }
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface) {
    (void)surface;

    struct display_output *output = data;
    if (VERBOSE)
        cflp_info("Destroying output %s (%s)", output->name, output->identifier);
    destroy_display_output(output);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

static void create_layer_surface(struct display_output *output) {
    output->surface = wl_compositor_create_surface(output->state->compositor);

    // Empty input region
    struct wl_region *input_region = wl_compositor_create_region(output->state->compositor);
    wl_surface_set_input_region(output->surface, input_region);
    wl_region_destroy(input_region);

    output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        output->state->layer_shell, output->surface, output->wl_output, output->state->surface_layer, "slapper");

    zwlr_layer_surface_v1_set_size(output->layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(output->layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
    );
    zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, -1);
    zwlr_layer_surface_v1_add_listener(output->layer_surface, &layer_surface_listener, output);
    
    // No video overlay setup needed - we'll render manually
    
    wl_surface_commit(output->surface);
}

static void output_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width,
        int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform) {
    // NOP
}

static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height,
        int32_t refresh) {
    // NOP
}

static void output_done(void *data, struct wl_output *wl_output) {
    (void)wl_output;

    struct display_output *output = data;

    bool name_ok = (strstr(output->state->monitor, output->name) != NULL) ||
            (strlen(output->identifier) != 0 && strstr(output->state->monitor, output->identifier) != NULL) ||
            // Keep for legacy reasons
            (strcmp(output->state->monitor, "*") == 0) ||
            // Let's just cover all cases here
            (strcmp(output->state->monitor, "ALL") == 0) ||
            (strcmp(output->state->monitor, "All") == 0) ||
            (strcmp(output->state->monitor, "all") == 0);
    if (name_ok && !output->layer_surface) {
        if (VERBOSE)
            cflp_info("Output %s (%s) selected", output->name, output->identifier);
        create_layer_surface(output);
    }
    if (!name_ok || (strcmp(output->state->monitor, "") == 0)) {
        if (SHOW_OUTPUTS)
            cflp_info("Output: %s  Identifier: %s", output->name, output->identifier);
        destroy_display_output(output);
    }
}

static void output_scale(void *data, struct wl_output *wl_output, int32_t scale) {
    struct display_output *output = data;
    output->scale = scale;
}

static void output_name(void *data, struct wl_output *wl_output, const char *name) {
    (void)wl_output;

    struct display_output *output = data;
    output->name = strdup(name);
}

static void output_description(void *data, struct wl_output *wl_output, const char *description) {
    (void)wl_output;

    struct display_output *output = data;

    // wlroots currently sets the description to `make model serial (name)`
    // Having `(name)` is redundant and must be removed to have a clean identifier.
    // If this changes in the future, this will need to be modified.
    char *paren = strrchr(description, '(');
    if (paren) {
        size_t length = paren - description;
        output->identifier = calloc(length, sizeof(char));
        if (!output->identifier) {
            cflp_warning("Failed to allocate output identifier");
            return;
        }
        strncpy(output->identifier, description, length);
        output->identifier[length - 1] = '\0';
    } else {
        output->identifier = strdup(description);
    }
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
        uint32_t version) {
    (void)version;

    struct wl_state *state = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct display_output *output = calloc(1, sizeof(struct display_output));
        output->scale = 1; // Default to no scaling
        output->state = state;
        output->wl_name = name;
        output->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 4);
        wl_output_add_listener(output->wl_output, &output_listener, output);
        wl_list_insert(&state->outputs, &output->link);

    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)registry;

    struct wl_state *state = data;
    struct display_output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &state->outputs, link) {
        if (output->wl_name == name) {
            cflp_info("Destroying output %s (%s)", output->name, output->identifier);
            destroy_display_output(output);
            break;
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

// Command line parsing helpers
static char **get_watch_list(char *path_name) {

    FILE *file = fopen(path_name, "r");
    if (file) {
        // Dynamically realloc for each program added to list
        char app[512];
        char **list = NULL;
        uint i = 0;
        for (i=0; fscanf(file, "%511s", app) != EOF; i++) {
            list = realloc(list, (i+1) * sizeof(char *));
            if (!list) {
                cflp_error("Failed to reallocate watch list");
                exit(EXIT_FAILURE);
            }
            list[i] = strdup(app);
        }
        // Null terminate
        list = realloc(list, (i+1) * sizeof(char *));
        list[i] = NULL;

        fclose(file);
        // If any app found
        if (list[0])
            return list;
    }
    return NULL;
}

static void copy_argv(int argc, char *argv[]) {
    halt_info.argc = argc;
    halt_info.argv_copy = calloc(argc+1, sizeof(char *));
    if (!halt_info.argv_copy) {
        cflp_error("Failed to allocate argv copy");
        exit(EXIT_FAILURE);
    }

    int j = 0;
    for (int i=0; i < argc; i++) {
        if (strcmp(argv[i], "-Z") == 0) { // Remove hidden opt
            i++; // Skip optind
            halt_info.argc -= 2;
        } else {
            halt_info.argv_copy[j] = strdup(argv[i]);
            j++;
        }
    }
}

static void set_watch_lists() {
    const char *home_dir = getenv("HOME");

    char *pause_path = NULL;
    if (asprintf(&pause_path, "%s/.config/mpvpaper/pauselist", home_dir) < 0) {
        cflp_error("Failed to create file path for pauselist");
        exit(EXIT_FAILURE);
    }
    halt_info.pauselist = get_watch_list(pause_path);
    free(pause_path);

    char *stop_path = NULL;
    if (asprintf(&stop_path, "%s/.config/mpvpaper/stoplist", home_dir) < 0) {
        cflp_error("Failed to create file path for stoplist");
        exit(EXIT_FAILURE);
    }
    halt_info.stoplist = get_watch_list(stop_path);
    free(stop_path);

    if (VERBOSE && halt_info.pauselist)
        cflp_info("pauselist found and will be monitored");
    if (VERBOSE && halt_info.stoplist)
        cflp_info("stoplist found and will be monitored");
}

// Command line parsing
static void parse_command_line(int argc, char **argv, struct wl_state *state) {

    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"help-output", no_argument, NULL, 'd'},
        {"verbose", no_argument, NULL, 'v'},
        {"fork", no_argument, NULL, 'f'},
        {"auto-pause", no_argument, NULL, 'p'},
        {"auto-stop", no_argument, NULL, 's'},
        {"slideshow", required_argument, NULL, 'n'},
        {"layer", required_argument, NULL, 'l'},
        {"gst-options", required_argument, NULL, 'o'},
        {"fps-cap", required_argument, NULL, 'r'},
        {0, 0, 0, 0}
    };

    const char *usage =
        "Usage: slapper [options] <output> <url|path filename>\n"
        "\n"
        "Example: slapper -vs -o \"no-audio loop\" DP-2 /path/to/video\n"
        "\n"
        "Options:\n"
        "--help         -h              Displays this help message\n"
        "--help-output  -d              Displays all available outputs and quits\n"
        "--verbose      -v              Be more verbose (-vv for higher verbosity)\n"
        "--fork         -f              Forks slapper so you can close the terminal\n"
        "--auto-pause   -p              Automagically* pause GStreamer when the wallpaper is hidden\n"
        "                               This saves CPU usage, more or less, seamlessly\n"
        "--auto-stop    -s              Automagically* stop GStreamer when the wallpaper is hidden\n"
        "                               This saves CPU/RAM usage, although more abruptly\n"
        "--slideshow    -n SECS         Slideshow mode plays the next video in a playlist every ? seconds\n"
        "--layer        -l LAYER        Specifies shell surface layer to run on (background by default)\n"
        "--gst-options  -o \"OPTIONS\"    Forwards GStreamer options (Must be within quotes\"\")\n"
        "--fps-cap      -r FPS           Frame rate cap (30, 60, or 100 FPS, default: 30)\n"
        "\n"
        "* The auto options might not work as intended\n"
        "See the man page for more details\n";

    char *layer_name;

    int opt;
    while ((opt = getopt_long(argc, argv, "hdvfpsn:l:o:r:Z:", long_options, NULL)) != -1) {

        switch (opt) {
            case 'h':
                fprintf(stdout, "%s", usage);
                exit(EXIT_SUCCESS);
            case 'd':
                SHOW_OUTPUTS = true;
                state->monitor = "";
                return;
            case 'v':
                VERBOSE += 1;
                break;
            case 'f':
                if (fork() > 0)
                    exit(EXIT_SUCCESS);

                fclose(stdout);
                fclose(stderr);
                fclose(stdin);
                break;
            case 'p':
                halt_info.auto_pause = 1;

                if (halt_info.auto_stop) {
                    cflp_warning("You cannot use auto-stop and auto-pause together");
                    halt_info.auto_stop = 0;
                }
                break;
            case 's':
                halt_info.auto_stop = 1;

                if (halt_info.auto_pause) {
                    cflp_warning("You cannot use auto-pause and auto-stop together");
                    halt_info.auto_pause = 0;
                }
                break;
            case 'n':
                SLIDESHOW_TIME = atoi(optarg);
                if (SLIDESHOW_TIME == 0)
                    cflp_warning("0 or invalid time set for slideshow. Please use a positive integer");
                break;
            case 'l':
                layer_name = strdup(optarg);
                if (layer_name == NULL) {
                    state->surface_layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
                } else if (strcasecmp(layer_name, "top") == 0) {
                    state->surface_layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
                } else if (strcasecmp(layer_name, "bottom") == 0) {
                    state->surface_layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
                } else if (strcasecmp(layer_name, "background") == 0) {
                    state->surface_layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
                } else if (strcasecmp(layer_name, "overlay") == 0) {
                    state->surface_layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
                } else {
                    cflp_error("%s is not a shell surface layer. Your options are: top, bottom, background and overlay", layer_name);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'o':
                gst_options = strdup(optarg);
                // Replace spaces with newlines
                for (int i=0; i < (int)strlen(gst_options); i++) {
                    if (gst_options[i] == ' ')
                        gst_options[i] = '\n';
                }
                break;
            case 'r':
                set_frame_rate_cap(atoi(optarg));
                break;

            case 'Z': // Hidden option to recover video pos after stopping
                halt_info.save_info = strdup(optarg);
                break;
        }
    }

    if (VERBOSE)
        cflp_info("Verbose Level %i enabled", VERBOSE);

    // Need at least a output and file or playlist file
    if (optind+1 >= argc) {
        cflp_error("Not enough args passed. Please set output and url|path filename");
        fprintf(stderr, "%s", usage);
        exit(EXIT_FAILURE);
    }

    state->monitor = strdup(argv[optind]);
    video_path = strdup(argv[optind+1]);
}

static void check_paper_processes() {
    // Check for other wallpaper process running
    const char *other_wallpapers[] = {"swaybg", "glpaper", "hyprpaper", "wpaperd", "swww-daemon"};
    char wallpaper_sbuffer[64] = {0};

    for (int i=0; i < sizeof(other_wallpapers) / sizeof(other_wallpapers[0]); i++) {
        snprintf(wallpaper_sbuffer, sizeof(wallpaper_sbuffer), "pidof %s > /dev/null", other_wallpapers[i]);

        if (!system(wallpaper_sbuffer))
            cflp_warning("%s is running. This may block slapper from being seen.", other_wallpapers[i]);
    }
}

// Main function
int main(int argc, char **argv) {
    // Initialize mtrace for memory leak detection
    mtrace();
    
    signal(SIGINT, handle_signal);
    signal(SIGQUIT, handle_signal);
    signal(SIGTERM, handle_signal);

    check_paper_processes();

    struct wl_state state = {0};
    wl_list_init(&state.outputs);
    global_state = &state;
    // Set default layer to background
    state.surface_layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;

    parse_command_line(argc, argv, &state);
    set_watch_lists();
    if (halt_info.auto_stop || halt_info.stoplist)
        copy_argv(argc, argv);

    // Create pipe for checking render_update_callback()
    if (pipe(wakeup_pipe) == -1) {
        cflp_error("Creating a self-pipe failed.");
        return EXIT_FAILURE;
    }
    fcntl(wakeup_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(wakeup_pipe[1], F_SETFD, FD_CLOEXEC);

    // Connect to Wayland compositor
    state.display = wl_display_connect(NULL);
    if (!state.display) {
        cflp_error("Unable to connect to the compositor. If your compositor is running, check or set the WAYLAND_DISPLAY environment variable.");
        return EXIT_FAILURE;
    }
    if (VERBOSE)
        cflp_success("Connected to Wayland compositor");

    // Don't start egl and gst if just displaying outputs
    if (!SHOW_OUTPUTS) {
        // Init render before outputs
        init_egl(&state);
        if (VERBOSE)
            cflp_success("EGL initialized");
        init_gst(&state);
        init_threads();
        if (VERBOSE)
            cflp_success("GStreamer initialized");
    }

    // Setup wayland surfaces
    struct wl_registry *registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(registry, &registry_listener, &state);
    wl_display_roundtrip(state.display);
    if (state.compositor == NULL || state.layer_shell == NULL) {
        cflp_error("Missing a required Wayland interface");
        return EXIT_FAILURE;
    }

    // Check outputs
    wl_display_roundtrip(state.display);
    if (SHOW_OUTPUTS)
        exit(EXIT_SUCCESS);
    if (wl_list_empty(&state.outputs)) {
        cflp_error(":/ sorry about this but we can't seem to find any output.");
        return EXIT_FAILURE;
    }

    // Main Loop
    while (true) {
        struct pollfd fds[2];
        fds[0].fd = wl_display_get_fd(state.display);
        fds[0].events = POLLIN;
        fds[1].fd = wakeup_pipe[0];
        fds[1].events = POLLIN;

        // First make sure to call wl_display_prepare_read() before poll() to avoid deadlock
        int wl_display_prepare_read_state = wl_display_prepare_read(state.display);

        // Next flush just before poll()
        if (wl_display_flush(state.display) == -1 && errno != EAGAIN)
            break;

        // Wait for a GStreamer callback or wl_display event
        if (poll(fds, sizeof(fds) / sizeof(fds[0]), 50) == -1 && errno != EINTR)
            break;

        // If wl_display_prepare_read() was successful as 0
        if (wl_display_prepare_read_state == 0) {
            // Read if we have wl_display events after poll()
            if (fds[0].revents & POLLIN) {
                wl_display_read_events(state.display);
            } else { // Otherwise we must cancel the read
                wl_display_cancel_read(state.display);
            }
        }
        // Lastly process wl_display events without blocking
        if (wl_display_dispatch_pending(state.display) == -1)
            break;

        if (halt_info.stop_render_loop) {
            halt_info.stop_render_loop = 0;
            sleep(2); // Wait at least 2 secs to be killed
        }

        // Handle frame rendering
        if (fds[1].revents & POLLIN) {
            // Empty the pipe
            char tmp[64];
            if (read(wakeup_pipe[0], tmp, sizeof(tmp)) == -1)
                break;

            // Draw frame for all outputs
            struct display_output *output;
            wl_list_for_each(output, &state.outputs, link) {
                // Redraw immediately if not waiting for frame callback
                if (output->frame_callback == NULL) {
                    // Avoid crash when output is destroyed
                    if (output->egl_window && output->egl_surface) {
                        if (VERBOSE == 2)
                            cflp_info("GStreamer is ready to render for %s", output->name);
                        render(output);
                    }
                } else {
                    output->redraw_needed = true;
                }
            }
        }
    }

    struct display_output *output, *tmp_output;
    wl_list_for_each_safe(output, tmp_output, &state.outputs, link) { destroy_display_output(output); }

    return EXIT_SUCCESS;
}