# Persistent Wallpapers in Wayland

This guide explains how to set up persistent wallpapers (both video and static images) that automatically restore after reboot or login in Wayland.

## Overview

Unlike X11, Wayland doesn't have a built-in wallpaper system. Wallpapers are managed by compositor-specific tools or utilities like gSlapper. To make wallpapers persistent across reboots, you need to configure automatic startup.

## Methods for Persistent Wallpapers

### Method 1: Systemd User Service (Recommended)

**Best for:** Automatic restoration, process management, integration with system lifecycle

**Advantages:**
- ✅ Automatic startup on login
- ✅ Automatic restart on failure
- ✅ State restoration (video position, pause state)
- ✅ Process lifecycle management
- ✅ Logging via journalctl
- ✅ Proper Wayland environment handling

**Setup:**

1. **Create environment file:**
```bash
mkdir -p ~/.config/gslapper
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" > ~/.config/gslapper/environment
```

2. **Set initial wallpaper (saves state):**
```bash
# Video wallpaper
gslapper -o "loop" DP-1 /path/to/video.mp4
# Press Ctrl+C to stop (state is saved automatically)

# Static image
gslapper -o "fill" DP-1 /path/to/image.jpg
# Press Ctrl+C to stop
```

3. **Create service file:**
```bash
mkdir -p ~/.config/systemd/user
```

Copy the example service file:
```bash
cp /usr/lib/systemd/user/gslapper.service ~/.config/systemd/user/gslapper.service
```

Or create manually (`~/.config/systemd/user/gslapper.service`):
```ini
[Unit]
Description=gSlapper Wallpaper Service
After=graphical-session.target
Wants=graphical-session.target

[Service]
Type=notify
ExecStart=/usr/bin/gslapper --systemd --restore '*'
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5

EnvironmentFile=%h/.config/gslapper/environment
Environment=XDG_RUNTIME_DIR=%t

[Install]
WantedBy=default.target
```

4. **Enable and start:**
```bash
systemctl --user daemon-reload
systemctl --user enable --now gslapper.service
```

**For per-monitor wallpapers:**
Use the template service (`gslapper@.service`):
```bash
systemctl --user enable --now gslapper@DP-1.service
systemctl --user enable --now gslapper@DP-3.service
```

**See also:** [Systemd Service Setup](../systemd-service-setup.md) for detailed instructions.

---

### Method 2: Shell Script in Startup Files

**Best for:** Simple setup, no systemd knowledge required, quick configuration

**Advantages:**
- ✅ Simple - just add to shell config
- ✅ Works on systems without systemd
- ✅ Full control over startup timing

**Disadvantages:**
- ❌ No automatic restart on failure
- ❌ No process management
- ❌ Manual state restoration needed
- ❌ Environment variables must be set manually

**Setup:**

1. **Add to shell config** (`~/.bashrc`, `~/.zshrc`, or `~/.config/fish/config.fish`):

**For bash/zsh:**
```bash
# Start gSlapper on login
if [ -z "$WAYLAND_DISPLAY" ]; then
    export WAYLAND_DISPLAY=wayland-0  # Adjust if needed
fi

# Restore wallpaper (requires state file to exist)
gslapper --restore DP-1 &
```

**For fish:**
```fish
# Start gSlapper on login
if not set -q WAYLAND_DISPLAY
    set -gx WAYLAND_DISPLAY wayland-0
end

# Restore wallpaper
gslapper --restore DP-1 &
```

2. **Set initial wallpaper first:**
```bash
gslapper -o "loop" DP-1 /path/to/video.mp4
# Press Ctrl+C (saves state)
```

**Note:** This method requires you to manually set the wallpaper once to create the state file.

---

### Method 3: Compositor-Specific Startup

**Best for:** Integration with compositor configuration

#### Hyprland

Add to `~/.config/hypr/hyprland.conf`:
```bash
exec-once = gslapper --restore DP-1
```

Or with a delay to ensure Wayland is ready:
```bash
exec-once = sleep 1 && gslapper --restore DP-1
```

#### Sway

Add to `~/.config/sway/config`:
```bash
exec gslapper --restore DP-1
```

#### River

Add to `~/.config/river/init`:
```bash
gslapper --restore DP-1 &
```

#### Niri

Add to `~/.config/niri/config.kdl`:
```kdl
spawn-at-startup {
    gslapper --restore DP-1
}
```

Or with a delay to ensure Wayland is ready:
```kdl
spawn-at-startup {
    sleep 1 && gslapper --restore DP-1
}
```

#### Other Compositors

Check your compositor's documentation for how to run commands on startup. Most support `exec`, `exec-once`, or `spawn-at-startup` directives.

**Advantages:**
- ✅ Integrated with compositor
- ✅ Starts with compositor session
- ✅ Simple configuration

**Disadvantages:**
- ❌ Compositor-specific (not portable)
- ❌ No automatic restart on failure
- ❌ Limited process management

---

### Method 4: Manual Restoration

**Best for:** Testing, one-time setup, troubleshooting

**Usage:**
```bash
# Restore from saved state
gslapper --restore DP-1

# Or set new wallpaper
gslapper -o "loop" DP-1 /path/to/video.mp4
```

**When to use:**
- Testing wallpaper settings
- One-time wallpaper changes
- Troubleshooting state restoration
- Temporary wallpapers

---

## State Management

### How State Files Work

gSlapper automatically saves wallpaper state when you stop it (Ctrl+C) or when it exits:

**State file location:**
```
~/.local/state/gslapper/state-DP-1.txt
~/.local/state/gslapper/state-DP-3.txt
~/.local/state/gslapper/state.txt  (for '*' output)
```

**Override with XDG_STATE_HOME:**
```bash
# If set, uses $XDG_STATE_HOME/gslapper/ instead
export XDG_STATE_HOME="$HOME/.local/state"
```

### State File Format

State files use simple key=value text format (human-readable):

```
version=1
output=DP-1
path=/path/to/wallpaper.mp4
is_image=false
options=loop panscan=0.8
position=123.45
paused=0
```

**Key descriptions:**
- `version` - State file format version (currently 1)
- `output` - Monitor name (e.g., "DP-1", "HDMI-1")
- `path` - Full path to video or image file
- `is_image` - Type indicator (0 = video, 1 = static image)
- `options` - GStreamer options string (e.g., "loop panscan=0.8")
- `position` - Video position in seconds (0.0 for images)
- `paused` - Pause state for videos (0 = playing, 1 = paused)

### State File Security

gSlapper uses multiple mechanisms to ensure state file integrity and security:

#### Atomic Writes
State is written to a temporary file first, then renamed:
1. Write to `state-<output>.txt.tmp`
2. Verify write completed successfully
3. Atomic rename: `state-<output>.txt.tmp` → `state-<output>.txt`

**Benefit:** Prevents partial/corrupted state files if power loss or crash occurs during write.

#### File Locking
State operations use file locking (`flock()`):
- Exclusive lock during writes
- Prevents concurrent writes from multiple processes
- Blocks until lock is available

**Benefit:** Prevents state corruption when multiple gSlapper instances access same file.

#### File Permissions
State files are set to `0600` (user read/write only):
- Owner: Read/Write
- Group: No access
- Others: No access

**Benefit:** Protects wallpaper paths from other users on shared systems.

### Manual State Management

**Save state manually:**
```bash
# Save for current wallpaper and exit
gslapper --save-state
```

**Restore from state:**
```bash
# Restore for specific output
gslapper --restore DP-1

# Restore from custom state file
gslapper --restore --state-file /tmp/my-state.txt DP-1
```

**Disable state saving:**
```bash
# Prevent automatic state saving on exit
gslapper --no-save-state -o "loop" DP-1 /path/to/video.mp4
```

**Note:** The `--save-state` flag is useful for:
- Testing state persistence
- Backing up current configuration
- Integrating with custom scripts
- Debugging state file issues

---

## Multi-Monitor Setup

### Same Wallpaper on All Monitors

**Systemd service:**
```ini
ExecStart=/usr/bin/gslapper --systemd --restore '*'
```

**Shell script:**
```bash
gslapper --restore '*' &
```

### Different Wallpapers per Monitor

**Systemd template service:**
```bash
# Set wallpapers
gslapper -o "loop" DP-1 /path/to/video1.mp4  # Ctrl+C
gslapper -o "fill" DP-3 /path/to/image.jpg  # Ctrl+C

# Enable services
systemctl --user enable --now gslapper@DP-1.service
systemctl --user enable --now gslapper@DP-3.service
```

**Shell script:**
```bash
gslapper --restore DP-1 &
gslapper --restore DP-3 &
```

---

## Comparison of Methods

| Method | Auto-start | Auto-restart | State Restore | Process Mgmt | Complexity |
|--------|------------|--------------|--------------|--------------|------------|
| **Systemd Service** | ✅ | ✅ | ✅ | ✅ | Medium |
| **Shell Script** | ✅ | ❌ | ✅ | ❌ | Low |
| **Compositor Config** | ✅ | ❌ | ✅ | ❌ | Low |
| **Manual** | ❌ | ❌ | ✅ | ❌ | Very Low |

---

## Troubleshooting

### Wallpaper Not Restoring

**Check state file exists:**
```bash
ls ~/.local/state/gslapper/
```

**Verify state file content:**
```bash
cat ~/.local/state/gslapper/state-DP-1.txt
```

**Check if path still exists:**
```bash
# From state file, verify the path is valid
test -f /path/to/wallpaper.mp4 && echo "Path exists" || echo "Path missing"
```

### Service Not Starting

**Check service status:**
```bash
systemctl --user status gslapper.service
```

**View logs:**
```bash
journalctl --user -u gslapper.service -f
```

**Verify environment file:**
```bash
cat ~/.config/gslapper/environment
# Should contain: WAYLAND_DISPLAY=wayland-0 (or wayland-1)
```

### Wayland Connection Issues

**Find your WAYLAND_DISPLAY:**
```bash
echo $WAYLAND_DISPLAY
```

**Update environment file:**
```bash
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" > ~/.config/gslapper/environment
```

**For shell scripts, export before running:**
```bash
export WAYLAND_DISPLAY=wayland-0
export XDG_RUNTIME_DIR=/run/user/$UID
gslapper --restore DP-1
```

---

## Best Practices

1. **Always set wallpaper manually first** - This creates the state file
2. **Use systemd service for production** - Best reliability and management
3. **Test state restoration** - Verify `gslapper --restore` works before enabling service
4. **Keep wallpaper paths stable** - Moving files breaks state restoration
5. **Use absolute paths** - Relative paths may not resolve correctly
6. **Check logs regularly** - `journalctl --user -u gslapper.service` for issues

---

## See Also

- [Systemd Service Setup](../systemd-service-setup.md) - Detailed systemd configuration
- [Video Wallpapers](./video-wallpapers.md) - Video wallpaper guide
- [Static Images](./static-images.md) - Static image guide
- [Multi-Monitor Setup](../advanced/multi-monitor.md) - Multi-monitor configuration
- [Command Line Options](./command-line-options.md) - All available options
