# TO_IMPLEMENT

This document tracks features, enhancements, and improvements that are planned or partially implemented but not yet complete.

---

## Table of Contents

- [Partially Implemented / Documented But Not Working](#partially-implemented--documented-but-not-working)
- [Not Implemented](#not-implemented)
- [Future Enhancements](#future-enhancements)
- [Performance Optimizations](#performance-optimizations)
- [Quality of Life Improvements](#quality-of-life-improvements)
- [Developer Experience](#developer-experience)

---

## Partially Implemented / Documented But Not Working

### 1. Slideshow Mode

**Status**: 🟡 Partially implemented
**Files**: `src/main.c` (lines 3203-3206)
**Issue**: `-n, --slideshow` option exists but lacks actual playlist functionality

**What's Implemented**:
- CLI flag parsing
- Timing parameter acceptance

**What's Missing**:
- Playlist file parsing
- Automatic wallpaper switching at intervals
- Support for multiple wallpapers in slideshow

**Priority**: Medium
**Estimated Effort**: 2-3 days

---

### 2. Transition Effects

**Status**: 🟡 Partially working
**Files**: `src/main.c` (lines 136-177, 624-851)
**Issue**: Fade transitions implemented but marked with several "FIXED" comments indicating previous bugs

**What's Implemented**:
- Fade transition shader
- Transition duration control
- Alpha blending between old and new textures

**Known Issues**:
- Multiple bug fixes documented in code
- May not work reliably in all scenarios
- Only works between static images, not videos

**Priority**: Medium (stabilize existing implementation)
**Estimated Effort**: 1-2 days

---

## Not Implemented

### 3. Preload Cache System

**Status**: 🔴 Not implemented
**Files**: `src/ipc.h` (struct `preload_entry_t`), TODO comments in `main.c` (lines 1996, 2004, 2009)

**What's Planned**:
- Preload images into memory (like hyprpaper)
- Cache rendered textures for instant switching
- List/clear cache via IPC commands

**IPC Commands Documented But Not Working**:
```bash
preload <path>   # TODO: Implement
unload <path>     # TODO: Implement
list             # TODO: Implement
```

**Data Structure Defined**:
```c
typedef struct preload_entry {
    char *path;
    gpointer frame_data;
    gsize frame_size;
    gint width;
    gint height;
    struct preload_entry *next;
} preload_entry_t;
```

**Priority**: High (frequently requested)
**Estimated Effort**: 3-5 days

**Design Considerations**:
- Memory vs speed tradeoff (large images = lots of VRAM)
- Cache invalidation strategy
- LRU cache eviction
- Configurable cache size limit

---

### 4. Playlist Support

**Status**: 🔴 Not implemented
**Related to**: Slideshow mode
**Use Cases**:
- Multiple videos in rotation
- Timed wallpaper changes
- Random selection from playlist

**Priority**: Medium
**Estimated Effort**: 2-3 days (depends on slideshow implementation)

---

### 5. Swaybg-Style Fill/Stretch Modes

**Status**: 🟡 Partially implemented
**Current State**: GStreamer has `fill`, `stretch`, `panscan` modes

**What's Missing**:
- Better integration of scaling options
- More granular control (e.g., exact fit vs slight crop)
- User-friendly names for common modes

**Priority**: Low
**Estimated Effort**: 1 day

---

## Future Enhancements

### 6. Multiple State Profiles

**Status**: 🔴 Not implemented
**Description**: Allow users to save/switch between multiple wallpaper configurations

**Use Cases**:
- Day/night themes (different wallpaper sets)
- Work/personal profiles
- Testing different configurations without overwriting state

**Implementation Ideas**:
- Named state files: `state-profile-<name>.txt`
- IPC command: `switch-profile <name>`
- CLI flag: `--profile <name>`

**Priority**: Low-Medium
**Estimated Effort**: 2-3 days

---

### 7. Scheduled Wallpapers

**Status**: 🔴 Not implemented
**Description**: Change wallpaper based on time of day

**Use Cases**:
- Light wallpapers during day
- Dark wallpapers at night
- Themed wallpapers based on schedule

**Implementation Ideas**:
- Cron-like scheduling
- Time-based state profiles
- Integration with system time/location

**Priority**: Low
**Estimated Effort**: 3-4 days

---

### 8. State File Encryption

**Status**: 🔴 Not implemented
**Description**: Optional encryption for sensitive wallpaper paths

**Use Cases**:
- Privacy for custom wallpaper paths
- Secure state storage on shared systems

**Implementation Ideas**:
- Use libgcrypt or sodium
- Password prompt on startup
- Encrypted state files (`.enc` extension)
- Optional via flag: `--encrypt-state`

**Priority**: Low
**Estimated Effort**: 2-3 days

---

### 9. Remote State Sync

**Status**: 🔴 Not implemented
**Description**: Sync state files across multiple machines

**Use Cases**:
- Multiple workstations with same setup
- Desktop + laptop synchronization
- Backup of wallpaper preferences

**Implementation Ideas**:
- Cloud storage sync (Nextcloud, Syncthing)
- Git-based config repo
- Custom sync server

**Priority**: Low
**Estimated Effort**: 4-5 days

---

## Performance Optimizations

### 10. playbin3 Migration

**Status**: 🟡 Evaluated - Not recommended yet
**Current State**: Using GStreamer `playbin` element
**Target**: Migrate to `playbin3` for improved gapless playback

**Key Differences (playbin vs playbin3)**:
- `playbin3` uses `uridecodebin3` internally (vs `uridecodebin` in `playbin`)
- Better gapless playback support with improved buffering
- More modern architecture with stream-aware parsing
- Single decoder instance per stream type (audio/video/text)

**Benefits**:
- Improved gapless playback (native support via `about-to-finish` signal)
- Better memory management during media transitions
- More predictable buffering behavior
- Modern GStreamer best practices

**Migration Complexity**: **Trivial**
- Simply change `gst_element_factory_make("playbin", ...)` to `gst_element_factory_make("playbin3", ...)`
- API is nearly identical for basic usage
- Existing `about-to-finish` signal still works

**GStreamer Version Required**: 1.18+ (widely available)

**Concerns**:
- `playbin3` may have slightly different buffering behavior
- Some edge cases may behave differently
- Current segment seek looping works well with `playbin`

**Recommendation**: 
Keep `playbin` for now. Current looping implementation using segment seeks works reliably.
Consider migration when:
1. Gapless playback between different videos is needed (playlist feature)
2. Users report buffering issues that `playbin3` would solve

**Priority**: Low (current implementation works well)
**Estimated Effort**: 1 day (including testing)

---

### 11. Abstract Socket Support

**Status**: 🟡 Evaluated - Recommended for future
**Current State**: Filesystem-based Unix socket at user-specified path (e.g., `/tmp/gslapper.sock`)
**Target**: Optional abstract socket namespace support

**What Are Abstract Sockets?**:
- Linux-specific Unix domain sockets that exist in abstract namespace
- Socket address starts with null byte (`\0`) instead of filesystem path
- Automatically cleaned up when process exits (no stale socket files)
- No filesystem permissions to manage

**Implementation Pattern** (from OpenSSH, uwsgi, etc.):
```c
// Current (filesystem-based)
addr.sun_path[0] = '/';  // Normal path like "/tmp/gslapper.sock"

// Abstract socket (prefix with @ in user input, convert to \0)
if (path[0] == '@') {
    addr.sun_path[0] = '\0';  // First byte is null
    memcpy(addr.sun_path + 1, path + 1, strlen(path));  // Rest of name after @
}
```

**Benefits**:
- No stale socket cleanup needed (automatic on process exit)
- No race conditions with socket file unlink
- No filesystem permission issues
- Slightly faster (no filesystem operations)

**Portability Concerns**:
- **Linux-only** - Abstract sockets do NOT exist on BSD, macOS, or other Unix systems
- Must keep filesystem socket as default/fallback

**Security Considerations**:
- Abstract sockets are visible to all processes in the network namespace
- No filesystem permissions to restrict access
- For gSlapper (user wallpaper), this is acceptable

**Recommended Implementation**:
```bash
# User specifies abstract socket with @ prefix
gslapper -I @gslapper DP-1 video.mp4

# Filesystem socket (default, portable)
gslapper -I /tmp/gslapper.sock DP-1 video.mp4
```

**Priority**: Low (filesystem sockets work fine, lifecycle handling already improved)
**Estimated Effort**: 1 day
**Blocking Issues**: None - can be added as optional feature

---

### 12. Adaptive Frame Rate

**Status**: 🔴 Not implemented
**Current State**: Fixed FPS cap (30, 60, or 100 FPS via `-r` flag)

**Improvement**: Automatically adjust FPS based on:
- CPU/GPU load
- Video playback needs
- User activity
- Battery/power state

**Priority**: Medium
**Estimated Effort**: 2-3 days

---

### 13. GPU Memory Management

**Status**: 🟡 Partially implemented (smart texture management exists)
**Current State**: Single video texture with dimension tracking

**Improvements**:
- Texture pool for multiple wallpapers
- Better VRAM usage prediction
- Automatic quality adjustment based on VRAM
- Compression for cached textures

**Priority**: Medium
**Estimated Effort**: 3-4 days

---

### 14. Hardware Acceleration Enhancements

**Status**: 🟡 GStreamer backend provides basic HW acceleration

**Improvements**:
- NVDEC/NVENC integration (NVIDIA)
- VAAPI integration (Intel/AMD)
- Better Wayland EGL integration
- Zero-copy video frame transfer

**Priority**: Low-Medium
**Estimated Effort**: 5-7 days

---

## Quality of Life Improvements

### 15. Enhanced Error Messages

**Status**: ✅ Implemented (2024-12)
**Current State**: Improved error messages with `strerror(errno)` details

**What Was Implemented**:
- File access errors include system error message
- Path validation with length checks
- IPC input validation with rejection of control characters
- `help` IPC command to list available commands

**Priority**: ~~Medium~~ Complete
**Estimated Effort**: ~~2-3 days~~ Done

---

### 16. Wallpaper Picker GUI

**Status**: 🔴 Not implemented
**Description**: Simple GUI for selecting wallpapers without CLI

**Features**:
- File browser with preview
- Monitor selector
- Quick apply button
- Save/load profiles (if implemented)

**Priority**: Low (CLI is primary interface)
**Estimated Effort**: 5-7 days (GTK or Qt)

**Alternative**: Keep CLI-focused, improve TUI experience instead

---

### 17. Better Logging

**Status**: 🟡 Basic logging exists (`cflp_*` functions)

**Improvements**:
- Log levels (debug, info, warning, error)
- Log rotation (size/time-based)
- Structured logging (JSON for parsing)
- Per-component verbosity

**Priority**: Medium
**Estimated Effort**: 1-2 days

---

### 18. Config File Support

**Status**: 🔴 Not implemented
**Current State**: Command-line arguments only

**Improvements**:
- `~/.config/gslapper/config` file
- Default options set in config
- Override CLI args with config
- Multiple config profiles

**Priority**: Medium
**Estimated Effort**: 2-3 days

**Example Config**:
```ini
[gslapper]
# Default scaling mode
default_mode = fill

# Default FPS cap
fps_cap = 30

# Default layer
layer = background

[monitors]
DP-1 = ~/Videos/wallpaper.mp4
HDMI-1 = ~/Pictures/wallpaper.jpg
```

---

### 19. Color Theme/Filter Support

**Status**: 🔴 Not implemented
**Description**: Apply color filters to wallpapers

**Features**:
- Grayscale filter
- Sepia tone
- Color tint
- Brightness/contrast adjustment

**Priority**: Low
**Estimated Effort**: 2-3 days (shader implementation)

---

## Developer Experience

### 20. Unit Tests

**Status**: 🟡 Partially implemented (2024-12)
**Current State**: Integration test suite added

**What's Implemented**:
- `tests/test_basic.sh` - 16 build verification tests
- `tests/test_memory.sh` - ASAN/Valgrind memory safety tests  
- `tests/test_systemd.sh` - Systemd service validation scenarios

**What's Still Missing**:
- Unit tests for state management functions
- Unit tests for IPC parsing logic
- Mock-based Wayland testing (complex)

**Priority**: ~~High~~ Medium (basic tests exist)
**Estimated Effort**: 3-5 days (for additional unit tests)

**Tools to Consider**:
- Check framework
- Unity test framework
- Custom simple test harness

---

### 21. CI/CD Pipeline

**Status**: 🔴 Not implemented (docs deployment exists, no testing)

**Components**:
- Automated testing on push
- Coverage reporting
- Static analysis (clang-tidy, cppcheck)
- Build artifact caching

**Priority**: Medium
**Estimated Effort**: 3-4 days

**Tools**:
- GitHub Actions (already has docs deploy)
- clang-tidy for static analysis
- valgrind for memory leaks
- AddressSanitizer

---

### 22. Debug Mode Enhancements

**Status**: 🔴 Not implemented
**Current State**: `-v` and `-vv` verbosity flags

**Improvements**:
- Debug-specific logging (render timing, GStreamer pipeline states)
- Frame-by-frame stepping
- Memory usage tracking
- Visual debugging overlays

**Priority**: Low
**Estimated Effort**: 2-3 days

---

### 23. Documentation Generator

**Status**: 🔴 Not implemented
**Description**: Auto-generate API docs from code comments

**Implementation**:
- Doxygen-style comments
- Auto-generation from source
- Keep docs in sync with code

**Priority**: Low
**Estimated Effort**: 2-3 days

---

## Security Hardening (Completed 2024-12)

### 24. Executable Path Resolution

**Status**: ✅ Implemented
**Files**: `src/main.c` (stop_slapper), `src/holder.c` (revive_slapper)

**What Was Fixed**:
- Safe `readlink()` with `PATH_MAX` buffer and null-termination
- Bounds checking before string operations
- NULL checks for all `strdup()` calls

---

### 25. IPC Socket Lifecycle

**Status**: ✅ Implemented  
**Files**: `src/ipc.c` (create_socket)

**What Was Fixed**:
- Connect-test before unlink (prevents hijacking active sockets)
- Input validation rejecting control characters
- Path length validation (max 4096 chars)

---

### 26. Shared Memory Names

**Status**: ✅ Implemented
**Files**: `src/holder.c` (create_dummy_buffer)

**What Was Fixed**:
- PID-based unique SHM names (`/gslapper-shm-%d`)
- Prevents collisions between instances

---

## Feature Gaps vs Competitors

### vs swww

| Feature | swww | gSlapper | Status |
|---------|-------|-----------|--------|
| Preload cache | ✅ | ❌ | TO IMPLEMENT #3 |
| Image browser | ✅ | ❌ | N/A (use external tools) |
| Color filters | ✅ | ❌ | TO IMPLEMENT #17 |

### vs mpvpaper

| Feature | mpvpaper | gSlapper | Status |
|---------|-----------|-----------|--------|
| Aspect ratio detection | ✅ | 🟡 Partial | Need better mode detection |
| EDID-based output info | ✅ | 🟡 Basic | TO IMPLEMENT #21 |

### vs hyprpaper

| Feature | hyprpaper | gSlapper | Status |
|---------|------------|-----------|--------|
| Preload cache | ✅ | ❌ | TO IMPLEMENT #3 |
| Daemon-based | ✅ | 🟡 Optional | Already supported via systemd |

---

## Implementation Priorities

### P0 - High Priority (Next 1-2 months)

1. **#3 - Preload Cache System** (Frequently requested, high impact)
2. **#21 - CI/CD Pipeline** (Leverage existing test scripts)

### P1 - Medium Priority (Next 3-6 months)

3. **#1 - Slideshow Mode** (Complete implementation)
4. **#2 - Transition Effects** (Stabilize existing code)
5. **#12 - Adaptive Frame Rate** (Performance improvement)
6. **#13 - GPU Memory Management** (Performance improvement)
7. **#17 - Better Logging** (Developer experience)
8. **#18 - Config File Support** (User convenience)

### P2 - Low Priority (Future / Nice to Have)

9. **#4 - Playlist Support** (Depends on slideshow)
10. **#6 - Multiple State Profiles** (Advanced use cases)
11. **#7 - Scheduled Wallpapers** (Advanced use cases)
12. **#8 - State File Encryption** (Niche use case)
13. **#9 - Remote State Sync** (Niche use case)
14. **#10 - playbin3 Migration** (Evaluated - not urgent, current impl works)
15. **#11 - Abstract Socket Support** (Evaluated - optional enhancement)
16. **#14 - Hardware Acceleration Enhancements** (Advanced, risky)
17. **#16 - Wallpaper Picker GUI** (Alternative interface)
18. **#19 - Color Theme/Filter Support** (Visual effects)
19. **#22 - Debug Mode Enhancements** (Developer experience)
20. **#23 - Documentation Generator** (Developer experience)

### Recently Completed (2024-12)

- ✅ **#15 - Enhanced Error Messages** - Detailed error messages with strerror()
- ✅ **#20 - Unit Tests** - Basic test suite (test_basic.sh, test_memory.sh, test_systemd.sh)
- ✅ **#24 - Executable Path Resolution** - Safe readlink/execv handling
- ✅ **#25 - IPC Socket Lifecycle** - Connect-test before unlink, input validation
- ✅ **#26 - Shared Memory Names** - PID-based unique names

---

## Contributing

If you want to implement any of these features:

1. **Check this document** - Someone might already be working on it
2. **Open an issue** - Discuss implementation approach before starting
3. **Reference existing code** - Follow established patterns in `src/`
4. **Update this document** - Mark feature as implemented with PR number

## See Also

- **[API Reference](./development/api-reference.md)** - Full API documentation
- **[Development Guide](./development/contributing.md)** - How to contribute
- **[Architecture](./development/architecture.md)** - System architecture
- **[State Management](./development/systemd-service.md)** - State system implementation
