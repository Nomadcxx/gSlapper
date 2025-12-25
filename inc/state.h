#ifndef STATE_H
#define STATE_H

#include <stdbool.h>

// Simple state structure mapping to existing globals
struct wallpaper_state {
    char *output;        // Monitor name (e.g., "DP-1")
    char *path;         // Video/image path
    bool is_image;      // true for images, false for videos
    char *options;       // GStreamer options string
    double position;    // Video position in seconds (0.0 for images)
    bool paused;        // Pause state (videos only)
};

// State file operations
char *get_state_file_path(const char *output_name);  // output_name can be NULL for default
int save_state_file(const char *path, const struct wallpaper_state *state);
int load_state_file(const char *path, struct wallpaper_state *state);
void free_wallpaper_state(struct wallpaper_state *state);

#endif // STATE_H
