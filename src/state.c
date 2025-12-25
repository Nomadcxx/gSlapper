#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>
#include "state.h"
#include "cflogprinter.h"

#define DEFAULT_STATE_DIR ".local/state/gslapper"
#define DEFAULT_STATE_FILE "state.txt"
#define STATE_FILE_VERSION 1
#define MAX_PATH_LEN 4096
#define MAX_LINE_LEN 1024

// Get state file path (SECURE - no system() calls)
// output_name: Monitor name (e.g., "DP-1") or NULL for default state file
char *get_state_file_path(const char *output_name) {
    const char *home = getenv("HOME");
    const char *xdg_state = getenv("XDG_STATE_HOME");
    char *state_dir = NULL;
    char *state_file = NULL;
    
    if (xdg_state) {
        if (asprintf(&state_dir, "%s/gslapper", xdg_state) < 0)
            return NULL;
    } else if (home) {
        if (asprintf(&state_dir, "%s/%s", home, DEFAULT_STATE_DIR) < 0)
            return NULL;
    } else {
        return NULL;
    }
    
    // Create directory using POSIX functions (SECURE - proper recursive creation)
    struct stat st = {0};
    if (stat(state_dir, &st) == -1) {
        // Recursively create all parent directories
        char *path = strdup(state_dir);
        if (!path) {
            free(state_dir);
            return NULL;
        }
        char *p = path;
        
        // Skip leading slash if absolute path
        if (*p == '/') p++;
        
        while ((p = strchr(p, '/')) != NULL) {
            *p = '\0';
            if (stat(path, &st) == -1) {
                if (mkdir(path, 0700) != 0 && errno != EEXIST) {
                    free(path);
                    free(state_dir);
                    return NULL;
                }
            }
            *p = '/';
            p++;
        }
        // Create final directory
        if (mkdir(state_dir, 0700) != 0 && errno != EEXIST) {
            free(path);
            free(state_dir);
            return NULL;
        }
        free(path);
    }
    
    // Include output name in filename if provided (for multi-monitor support)
    if (output_name && strlen(output_name) > 0) {
        // Sanitize output name for use in filename
        char safe_name[256];
        strncpy(safe_name, output_name, sizeof(safe_name) - 1);
        safe_name[sizeof(safe_name) - 1] = '\0';
        // Replace invalid filename characters with underscore
        for (char *p = safe_name; *p; p++) {
            if (*p == '/' || *p == '\\' || *p == ':' || *p == '*') *p = '_';
        }
        if (asprintf(&state_file, "%s/state-%s.txt", state_dir, safe_name) < 0) {
            free(state_dir);
            return NULL;
        }
    } else {
        // Default name for single-instance or '*' mode
        if (asprintf(&state_file, "%s/%s", state_dir, DEFAULT_STATE_FILE) < 0) {
            free(state_dir);
            return NULL;
        }
    }
    
    free(state_dir);
    return state_file;
}

// Save state to simple text file (ATOMIC WRITE - prevents corruption)
int save_state_file(const char *path, const struct wallpaper_state *state) {
    if (!path || !state || !state->path) return -1;
    
    // Use atomic write: write to temp file, then rename (atomic on POSIX)
    char temp_path[MAX_PATH_LEN];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    
    // Open temp file with exclusive lock (use O_CREAT|O_EXCL for atomic lock file)
    int fd = open(temp_path, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, 0600);
    if (fd < 0) {
        cflp_error("Failed to create temp state file: %s", strerror(errno));
        return -1;
    }
    
    // Acquire exclusive lock (prevents concurrent writes)
    if (flock(fd, LOCK_EX) != 0) {
        cflp_error("Failed to lock state file: %s", strerror(errno));
        close(fd);
        unlink(temp_path);
        return -1;
    }
    
    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        cflp_error("Failed to open temp state file for writing");
        flock(fd, LOCK_UN);
        close(fd);
        unlink(temp_path);
        return -1;
    }
    
    // Write state in simple format with version
    fprintf(fp, "# gSlapper state file\n");
    fprintf(fp, "# Format: key=value\n");
    fprintf(fp, "version=%d\n", STATE_FILE_VERSION);
    fprintf(fp, "\n");
    
    if (state->output)
        fprintf(fp, "output=%s\n", state->output);
    if (state->path)
        fprintf(fp, "path=%s\n", state->path);
    fprintf(fp, "type=%s\n", state->is_image ? "image" : "video");
    if (state->options && strlen(state->options) > 0)
        fprintf(fp, "options=%s\n", state->options);
    if (!state->is_image) {
        fprintf(fp, "position=%.2f\n", state->position);
        fprintf(fp, "paused=%d\n", state->paused ? 1 : 0);
    }
    
    fflush(fp);
    fsync(fd);
    
    // Release lock and close
    flock(fd, LOCK_UN);
    fclose(fp);
    
    // Atomic rename (replaces old file atomically)
    if (rename(temp_path, path) != 0) {
        cflp_error("Failed to rename temp state file: %s", strerror(errno));
        unlink(temp_path);
        return -1;
    }
    
    cflp_success("State saved to %s", path);
    return 0;
}

// Load state from simple text file
int load_state_file(const char *path, struct wallpaper_state *state) {
    if (!path || !state) return -1;
    
    // Initialize state
    memset(state, 0, sizeof(struct wallpaper_state));
    
    if (access(path, R_OK) != 0) {
        // State file not found is not an error - just return
        return -1;
    }
    
    // Open and lock file
    FILE *fp = fopen(path, "r");
    if (!fp) {
        cflp_warning("Failed to open state file: %s", path);
        return -1;
    }
    
    int fd = fileno(fp);
    if (flock(fd, LOCK_SH) != 0) {
        cflp_warning("Failed to lock state file: %s", strerror(errno));
        fclose(fp);
        return -1;
    }
    
    // Parse simple key=value format
    char line[MAX_LINE_LEN];
    int version = 0;
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Remove newline
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Parse key=value
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals = '\0';
        char *key = line;
        char *value = equals + 1;
        
        // Set state fields with validation
        if (strcmp(key, "version") == 0) {
            version = atoi(value);
            if (version > STATE_FILE_VERSION) {
                cflp_warning("State file version %d is newer than supported %d", version, STATE_FILE_VERSION);
                // Continue anyway - be forward compatible
            }
        } else if (strcmp(key, "output") == 0) {
            state->output = strdup(value);
        } else if (strcmp(key, "path") == 0) {
            state->path = strdup(value);
        } else if (strcmp(key, "type") == 0) {
            if (strcmp(value, "image") == 0) {
                state->is_image = true;
            } else if (strcmp(value, "video") == 0) {
                state->is_image = false;
            } else {
                cflp_warning("Invalid type in state file: %s (expected 'image' or 'video')", value);
                flock(fd, LOCK_UN);
                fclose(fp);
                free_wallpaper_state(state);
                return -1;
            }
        } else if (strcmp(key, "options") == 0) {
            state->options = strdup(value);
        } else if (strcmp(key, "position") == 0) {
            state->position = strtod(value, NULL);
            if (state->position < 0.0) {
                cflp_warning("Invalid position in state file: %.2f (must be >= 0)", state->position);
                state->position = 0.0;
            }
        } else if (strcmp(key, "paused") == 0) {
            if (strcmp(value, "0") == 0) {
                state->paused = false;
            } else if (strcmp(value, "1") == 0) {
                state->paused = true;
            } else {
                cflp_warning("Invalid paused value in state file: %s (expected '0' or '1')", value);
                state->paused = false;
            }
        }
    }
    
    flock(fd, LOCK_UN);
    fclose(fp);
    
    // Validate required fields
    if (!state->path) {
        cflp_warning("State file missing required 'path' field");
        free_wallpaper_state(state);
        return -1;
    }

    // VALIDATION: Check if path exists
    if (state->path) {
        struct stat path_stat;
        if (stat(state->path, &path_stat) != 0) {
            cflp_warning("State file references non-existent path: %s", state->path);
            cflp_warning("File may have been moved or deleted since state was saved");
            // Don't fail - just warn and let caller decide
        }
    }

    // VALIDATION: Output name not empty
    if (state->output && strlen(state->output) == 0) {
        cflp_warning("Invalid empty output name in state file");
        free(state->output);
        state->output = NULL;
    }

    // Validate type is set
    // (is_image is set during parsing, so if we got here it's valid)

    cflp_success("State loaded from %s", path);
    return 0;
}

// Free state structure
void free_wallpaper_state(struct wallpaper_state *state) {
    if (!state) return;
    free(state->output);
    free(state->path);
    free(state->options);
    memset(state, 0, sizeof(struct wallpaper_state));
}
