# Systemd Service (Development Documentation)

## Overview

gSlapper includes systemd service integration for automatic wallpaper restoration on login. This is **optional** - gSlapper works perfectly fine without systemd.

## Implementation Details

### Architecture

**Two-Component Design**:
1. **Main Program** (`gslapper` binary) - Runs wallpaper display
2. **Helper Process** (`gslapper-holder`) - Gatekeeper for stoplist checking

**Why Helper Process?**
- Separates stoplist monitoring from main event loop
- Allows graceful lifecycle transitions
- Manages process restarts (like mpvpaper's approach)

### Service Modes

#### User Service Mode (`--systemd`)

**Behavior**:
- Sends `sd_notify(READY=1)` when wallpaper is displayed
- Saves state on `SIGHUP` (reload signal)
- Exits cleanly on `SIGTERM` (systemd shutdown)

**Activation**:
```c
#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>

// Notify systemd when ready
sd_notify(0, "READY=1\nSTATUS=Wallpapers loaded and playing");

// Notify systemd on shutdown
sd_notify(0, "STOPPING=1\nSTATUS=Shutting down");
#endif
```

#### Manual Mode (default)

**Behavior**:
- Runs until `SIGINT` (Ctrl+C) or `SIGTERM`
- Saves state on exit (if not disabled with `--no-save-state`)
- No systemd notifications

---

## State Management in Systemd Service

### State Saving Triggers

1. **Automatic on Exit**: When service stops (SIGTERM, SIGHUP)
2. **Explicit Save**: Via IPC `save-state` command or `--save-state` flag
3. **SIGHUP Reload**: Service reload triggers state save + restart

### State File Format

**Location**: `~/.local/state/gslapper/state-<output>.txt`

**Format** (Simple key=value, not JSON):
```
version=1
output=DP-1
path=/home/user/Videos/wallpaper.mp4
is_image=false
options=loop panscan=0.8
position=123.45
paused=0
```

### State File Operations

```c
// From state.c
char *get_state_file_path(const char *output_name);
int save_state_file(const char *path, const struct wallpaper_state *state);
int load_state_file(const char *path, struct wallpaper_state *state);
void free_wallpaper_state(struct wallpaper_state *state);
```

**Safety Features**:
- **Atomic writes**: Write to `.tmp` file, then rename
- **File locking**: `flock()` prevents concurrent writes
- **Permissions**: State files set to `0600` (user-only)
- **Backups**: Automatic backup before overwriting

---

## Service File Configuration

### Single Service (All Monitors)

**File**: `/usr/lib/systemd/user/gslapper.service`

```ini
[Unit]
Description=gSlapper Wallpaper Service
Documentation=https://nomadcxx.github.io/gSlapper/
After=graphical-session.target
Wants=graphical-session.target

[Service]
Type=notify
ExecStart=/usr/bin/gslapper --systemd --restore '*'
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5
TimeoutStopSec=30

EnvironmentFile=%h/.config/gslapper/environment
Environment=XDG_RUNTIME_DIR=%t

MemoryMax=512M
CPUQuota=50%
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=default.target
```

**Key Directives**:
- `Type=notify` - Waits for `sd_notify(READY=1)` before considering service started
- `ExecStart` - Uses `--systemd --restore` flags
- `ExecReload` - Sends `SIGHUP` for state save + restart
- `EnvironmentFile` - Loads `WAYLAND_DISPLAY` from user config
- `Environment=XDG_RUNTIME_DIR=%t` - Sets Wayland runtime directory
- `MemoryMax`, `CPUQuota` - Resource limits for safety

### Per-Monitor Template

**File**: `/usr/lib/systemd/user/gslapper@.service`

```ini
[Unit]
Description=gSlapper Wallpaper Service for %i
Documentation=https://nomadcxx.github.io/gSlapper/
After=graphical-session.target
Wants=graphical-session.target

[Service]
Type=notify
ExecStart=/usr/bin/gslapper --systemd --restore %i
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5
TimeoutStopSec=30

EnvironmentFile=%h/.config/gslapper/environment
Environment=XDG_RUNTIME_DIR=%t

MemoryMax=512M
CPUQuota=50%
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=default.target
```

**Usage**:
```bash
# Enable per monitor
systemctl --user enable --now gslapper@DP-1.service
systemctl --user enable --now gslapper@DP-2.service

# Each instance has separate state file:
# ~/.local/state/gslapper/state-DP-1.txt
# ~/.local/state/gslapper/state-DP-2.txt
```

---

## Build System Integration

### Optional systemd Dependency

**meson.build**:
```meson
# Optional systemd support
systemd_dep = dependency('libsystemd', required: false)
if systemd_dep.found()
  add_project_arguments('-DHAVE_SYSTEMD', language: 'c')
  message('Systemd support enabled')
else
  message('Systemd support disabled (libsystemd not found)')
endif
```

**Compilation Flags**:
- `HAVE_SYSTEMD` - Defined when libsystemd is found
- Code wraps systemd calls in `#ifdef HAVE_SYSTEMD`

### Service File Installation

**PKGBUILD**:
```bash
package() {
    # ... existing install commands ...

    # Install systemd user service files
    install -Dm644 gslapper.service \
        "${pkgdir}/usr/lib/systemd/user/gslapper.service"
    install -Dm644 gslapper@.service \
        "${pkgdir}/usr/lib/systemd/user/gslapper@.service"
}
```

---

## Environment Variables

### Required Environment

**`WAYLAND_DISPLAY`** (Required):
- Wayland display socket (e.g., `wayland-0`, `wayland-1`)
- Must be set for gSlapper to connect to compositor

**Configuration**:
```bash
# Create environment file
mkdir -p ~/.config/gslapper
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" > ~/.config/gslapper/environment
```

### Optional Environment

**`XDG_RUNTIME_DIR`** (Automatically set by systemd):
- User runtime directory (e.g., `/run/user/1000`)
- Used for Wayland socket access
- Set automatically by systemd service (`Environment=XDG_RUNTIME_DIR=%t`)

**`XDG_STATE_HOME`** (Optional):
- Override default state directory
- If set: `$XDG_STATE_HOME/gslapper/`
- If not set: `$HOME/.local/state/gslapper/`

---

## Service Lifecycle

### Startup Sequence

1. **Service Start** (systemd)
2. **Load Environment** - Read `~/.config/gslapper/environment`
3. **Load State** - Read from `~/.local/state/gslapper/state-*.txt`
4. **Initialize Wayland** - Connect to compositor
5. **Initialize GStreamer** - Set up pipeline for video/image
6. **Display Wallpaper** - Render to layer surface
7. **Notify Ready** - `sd_notify(READY=1)`
8. **Service Active** (systemd considers service started)

### Reload Sequence (SIGHUP)

1. **Receive Signal** - Systemd sends `SIGHUP`
2. **Save Current State** - Write position, pause state, options
3. **Stop GStreamer** - Clean shutdown of pipeline
4. **Reload State** - Load saved state from file
5. **Restart Pipeline** - Initialize GStreamer with saved config
6. **Resume Playback** - Continue from saved position
7. **Service Active** (systemd continues service)

### Shutdown Sequence (SIGTERM)

1. **Receive Signal** - Systemd sends `SIGTERM`
2. **Notify Stopping** - `sd_notify(STOPPING=1)`
3. **Save Current State** - Write final state
4. **Stop GStreamer** - Clean shutdown
5. **Cleanup Wayland** - Destroy surfaces, close display
6. **Exit** - Process terminates
7. **Service Stopped** (systemd marks service as stopped)

---

## Multi-Service Deployment

### Single Service (Same Wallpaper All Monitors)

**When to use**:
- Same wallpaper on all monitors
- Simplified setup
- Single state file

**Configuration**:
```bash
# Enable single service
systemctl --user enable --now gslapper.service

# State file:
# ~/.local/state/gslapper/state.txt (for '*' output)
```

### Multiple Services (Different Wallpaper Per Monitor)

**When to use**:
- Different wallpapers on each monitor
- Independent control per monitor
- Per-monitor state management

**Configuration**:
```bash
# Disable single service if running
systemctl --user disable gslapper.service

# Enable per-monitor services
systemctl --user enable --now gslapper@DP-1.service
systemctl --user enable --now gslapper@DP-2.service
systemctl --user enable --now gslapper@DP-3.service

# Each has its own state file:
# ~/.local/state/gslapper/state-DP-1.txt
# ~/.local/state/gslapper/state-DP-2.txt
# ~/.local/state/gslapper/state-DP-3.txt
```

**State Files**:
- Each instance reads/writes its own state file
- No conflicts between instances
- Independent video positions per monitor

---

## Troubleshooting (Development)

### Service Fails to Start

**Check logs**:
```bash
journalctl --user -u gslapper.service -n 50
```

**Common issues**:
1. **Missing `WAYLAND_DISPLAY`**:
   ```
   Error: Unable to connect to compositor
   ```
   **Fix**: Verify `~/.config/gslapper/environment` contains correct value

2. **No state file** (when using `--restore`):
   ```
   Warning: State file not found, starting fresh
   ```
   **Fix**: Not an error, service will start with no wallpaper

3. **File not found** (path in state file):
   ```
   Error: File does not exist: /path/to/wallpaper.mp4
   ```
   **Fix**: Update wallpaper path or fix state file

### Service Starts But No Wallpaper

**Debug steps**:
```bash
# Check service status
systemctl --user status gslapper.service

# View full logs
journalctl --user -u gslapper.service -f

# Check state file
cat ~/.local/state/gslapper/state-*.txt

# Verify file exists
test -f /path/to/wallpaper.mp4 && echo "OK" || echo "Missing"
```

### State Not Persisting

**Check**:
```bash
# Verify state directory exists and is writable
ls -la ~/.local/state/gslapper/

# Check state file permissions
stat ~/.local/state/gslapper/state-*.txt

# Manual test
gslapper --save-state
# Should save to state file
```

### systemd Support Disabled

**If building without libsystemd**:
```
Systemd support disabled (libsystemd not found)
```

**Result**:
- `--systemd` flag still works but no `sd_notify()` calls
- Service will still work but `Type=notify` won't wait for READY
- Consider using `Type=simple` instead

---

## Comparison with swww

### swww-daemon Architecture

**Single Process**:
- `swww-daemon` runs continuously
- Manages all outputs in one process
- Automatic cache restoration on startup
- No explicit state file (uses shared memory)

**Service File**:
```ini
[Service]
Type=simple
ExecStart=/usr/bin/swww-daemon
Restart=on-failure
```

### gSlapper Architecture

**One Process Per Wallpaper** (or One Process for All):
- Each service instance manages one output
- Explicit state file persistence
- State restoration via `--restore` flag
- More detailed state (video position, pause state, options)

**Service File**:
```ini
[Service]
Type=notify
ExecStart=/usr/bin/gslapper --systemd --restore <output>
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
```

### Key Differences

| Aspect | swww | gSlapper |
|---------|-------|-----------|
| **Process Model** | Single daemon | One process per wallpaper (optional) |
| **State Storage** | In-memory cache | File-based (text) |
| **Restoration** | Automatic on daemon start | Explicit `--restore` flag |
| **Systemd Type** | `simple` | `notify` (wait for ready) |
| **Reload Support** | No | Yes (`SIGHUP`) |
| **State Detail** | Cache of images | Full config + video position |

---

## Future Enhancements

**Planned** (see TO_IMPLEMENT.md):
1. **Playlist Support** - Rotate through multiple wallpapers
2. **Scheduled Wallpapers** - Change based on time of day
3. **State Encryption** - Optional encryption for sensitive paths
4. **Multiple State Profiles** - Switch between wallpaper configurations

---

## See Also

- **[Systemd Service Setup (User Guide)](../systemd-service-setup.md)** - End-user setup guide
- **[State Management (Implementation)](#state-management-in-systemd-service)** - State file API
- **[API Reference](./api-reference.md)** - Full API documentation
- **[Architecture](./architecture.md)** - Overall system architecture
- **[TO_IMPLEMENT.md](../../TO_IMPLEMENT.md)** - Future enhancements
