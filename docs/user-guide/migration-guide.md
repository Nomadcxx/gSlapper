# Migration Guide

This guide helps users migrate from other wallpaper tools to gSlapper. Whether you're coming from swww, mpvpaper, hyprpaper, swaybg, or feh, we'll show you how to make the switch smoothly.

## Why Migrate to gSlapper?

**Key Advantages**:
- ✅ **Video + Static Support** - One tool for both animated and static wallpapers
- ✅ **GStreamer Backend** - More efficient than libmpv, better NVIDIA compatibility
- ✅ **Minimal Resource Usage** - Smart texture management and frame rate limiting
- ✅ **Wayland Native** - Built for wlroots compositors (Hyprland, Sway, River, Niri)
- ✅ **IPC Control** - Runtime control via Unix sockets
- ✅ **Persistent State** - Automatic wallpaper restoration on login
- ✅ **Smooth Transitions** - Fade effects between static images

---

## Migrating from swww

### swww vs gSlapper Comparison

| Feature | swww | gSlapper |
|---------|-------|-----------|
| **Static Images** | ✅ | ✅ |
| **Video Wallpapers** | ❌ (images only) | ✅ |
| **Daemon** | `swww-daemon` | Optional systemd service |
| **Transitions** | ✅ (fade) | ✅ (fade) |
| **Scaling** | Limited | Full (fill, stretch, original, panscan) |
| **Multi-Monitor** | ✅ | ✅ |
| **IPC Control** | ✅ | ✅ |
| **Resource Usage** | Medium | Low (smart management) |
| **Video Support** | GIF only | Full GStreamer (MP4, MKV, WebM) |

### swww Commands → gSlapper Equivalents

#### Basic Usage

**swww:**
```bash
# Set wallpaper on all monitors
swww img /path/to/image.jpg

# Set on specific monitor
swww img -o DP-1 /path/to/image.jpg

# With transition
swww img -t fade --transition-step 90 /path/to/image.jpg
```

**gSlapper (static images):**
```bash
# Set wallpaper on all monitors
gslapper -o "fill" '*' /path/to/image.jpg

# Set on specific monitor
gslapper -o "fill" DP-1 /path/to/image.jpg

# With transition
gslapper -o "fill" --transition-type fade --transition-duration 0.5 DP-1 /path/to/image.jpg
```

#### Video Wallpapers (NEW in gSlapper)

```bash
# Set video wallpaper with looping
gslapper -o "loop" DP-1 /path/to/video.mp4

# Video with custom scaling
gslapper -o "loop panscan=0.8" DP-1 /path/to/video.mp4
```

#### Scaling Modes

| swww | gSlapper | Description |
|-------|-----------|-------------|
| `--no-resize` | `-o "original"` | Native resolution |
| `--cover` | `-o "fill"` | Cover screen, crop excess |
| `--contain` | `-o "panscan=1.0"` | Fit inside, keep aspect |

#### Daemon/Service

**swww:**
```bash
# Start daemon manually
swww-daemon

# Or via systemd
systemctl --user enable swww-daemon.service
systemctl --user start swww-daemon.service
```

**gSlapper:**
```bash
# No daemon needed for static images (one-shot like swww init)
# But you can use systemd for persistence:

# Create environment file
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" > ~/.config/gslapper/environment

# Enable service
systemctl --user enable --now gslapper.service
```

#### Cache Management

**swww:**
```bash
swww clear-cache
swww img --no-cache /path/to/image.jpg
```

**gSlapper:**
```bash
# gSlapper doesn't use a persistent cache
# Transitions are handled in-memory
# TODO: Preload cache coming soon (see TO_IMPLEMENT.md)
```

### swww Config → gSlapper

**swww config (`~/.config/swww/init.sh`):**
```bash
#!/bin/bash
swww init
swww img /path/to/wallpaper.jpg
```

**gSlapper equivalent:**
```bash
#!/bin/bash
# Method 1: One-shot (no daemon)
gslapper -o "fill" '*' /path/to/wallpaper.jpg

# Method 2: Systemd service for persistence
# See: Persistent Wallpapers guide
```

### Migration Checklist - swww Users

- [ ] Replace `swww img` commands with `gslapper -o "fill"` for static images
- [ ] Use `gslapper -o "loop"` for video wallpapers (new capability)
- [ ] Update compositor startup scripts
  - **Hyprland**: Change `exec-once = swww img ...` to `exec-once = gslapper ...`
  - **Sway**: Change `exec swww img ...` to `exec gslapper ...`
- [ ] Disable `swww-daemon` if using gSlapper service:
  ```bash
  systemctl --user disable swww-daemon.service
  ```
- [ ] Update shell aliases (if any):
  ```bash
  # Old
  alias wp='swww img ~/Pictures/wallpaper.jpg'

  # New
  alias wp='gslapper -o "fill" ~/Pictures/wallpaper.jpg'
  ```

---

## Migrating from mpvpaper

### mpvpaper vs gSlapper Comparison

| Feature | mpvpaper | gSlapper |
|---------|-----------|-----------|
| **Static Images** | ✅ | ✅ |
| **Video Wallpapers** | ✅ | ✅ |
| **Backend** | libmpv | GStreamer |
| **Memory Leaks** | ⚠️ Issues on NVIDIA | ✅ Fixed |
| **GPU Resource Usage** | High | Optimized |
| **Multi-Monitor** | ✅ | ✅ |
| **IPC Control** | ✅ | ✅ |
| **Transitions** | ❌ | ✅ |
| **Scaling Modes** | Limited | Full (fill, stretch, original, panscan) |
| **Persistent State** | No | ✅ Automatic |

### Why Switch from mpvpaper?

**Known Issues Fixed in gSlapper:**
- ✅ **NVIDIA Memory Leaks** - GStreamer backend solves libmpv memory issues
- ✅ **GPU Resource Usage** - Smart texture management reduces VRAM consumption
- ✅ **Static Image Transitions** - Fade effects not available in mpvpaper
- ✅ **Persistent State** - Automatic wallpaper restoration, no manual scripts needed

### mpvpaper Commands → gSlapper Equivalents

#### Basic Usage

**mpvpaper:**
```bash
# Video wallpaper
mpvpaper -f --loop DP-1 /path/to/video.mp4

# Video with options
mpvpaper -f --loop --panscan=1.0 DP-1 /path/to/video.mp4

# Static image
mpvpaper DP-1 /path/to/image.jpg
```

**gSlapper:**
```bash
# Video wallpaper
gslapper -f -o "loop" DP-1 /path/to/video.mp4

# Video with options
gslapper -f -o "loop panscan=1.0" DP-1 /path/to/video.mp4

# Static image (fill mode by default)
gslapper DP-1 /path/to/image.jpg

# Or explicit fill mode
gslapper -o "fill" DP-1 /path/to/image.jpg
```

#### Scaling/Panscan

**mpvpaper:**
```bash
# Full stretch
mpvpaper -f DP-1 video.mp4

# Panscan (0.0 = fit, 1.0 = fill)
mpvpaper -f --panscan=0.5 DP-1 video.mp4
```

**gSlapper:**
```bash
# Full stretch
gslapper -f -o "stretch" DP-1 video.mp4

# Panscan (0.0 = fit, 1.0 = fill)
gslapper -f -o "panscan=0.5" DP-1 video.mp4
```

#### Auto-Pause/Stop

**mpvpaper:**
```bash
# Uses ~/.config/mpvpaper/pauselist and stoplist
# Add application names to files
```

**gSlapper:**
```bash
# Same configuration files
echo "firefox" >> ~/.config/mpvpaper/pauselist
echo "games" >> ~/.config/mpvpaper/stoplist

# Or use command-line flags
gslapper -p -o "loop" DP-1 video.mp4  # Auto-pause
gslapper -s -o "loop" DP-1 video.mp4  # Auto-stop
```

#### IPC Control

**mpvpaper:**
```bash
# No built-in IPC
# Requires external tools or scripts
```

**gSlapper:**
```bash
# Start with IPC socket
gslapper -I /tmp/gslapper.sock -o "loop" DP-1 video.mp4

# Control via IPC
echo "pause" | nc -U /tmp/gslapper.sock
echo "resume" | nc -U /tmp/gslapper.sock
echo "change /new/video.mp4" | nc -U /tmp/gslapper.sock
```

### mpvpaper Systemd → gSlapper

**mpvpaper service:**
```ini
[Unit]
Description=mpvpaper service
After=graphical-session.target

[Service]
Type=simple
ExecStart=/usr/bin/mpvpaper -f --loop DP-1 /path/to/video.mp4
Restart=on-failure

[Install]
WantedBy=default.target
```

**gSlapper equivalent:**
```ini
[Unit]
Description=gSlapper Wallpaper Service
After=graphical-session.target
Wants=graphical-session.target

[Service]
Type=notify
ExecStart=/usr/bin/gslapper --systemd --restore DP-1
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

**Key Differences:**
- `--systemd --restore` enables state persistence
- `Type=notify` for systemd readiness
- Resource limits for better management
- SIGHUP reload support for state saving

### Migration Checklist - mpvpaper Users

- [ ] Replace `mpvpaper` with `gslapper` in all scripts
- [ ] Update systemd services
  ```bash
  systemctl --user disable mpvpaper.service
  systemctl --user enable gslapper.service
  ```
- [ ] Create environment file for gSlapper:
  ```bash
  mkdir -p ~/.config/gslapper
  echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" > ~/.config/gslapper/environment
  ```
- [ ] Test video playback (should work better on NVIDIA)
- [ ] Try fade transitions with static images
- [ ] Set up IPC socket for runtime control
- [ ] Update compositor startup scripts

---

## Migrating from hyprpaper

### hyprpaper vs gSlapper Comparison

| Feature | hyprpaper | gSlapper |
|---------|------------|-----------|
| **Static Images** | ✅ | ✅ |
| **Video Wallpapers** | ❌ | ✅ |
| **Hyprland Only** | ✅ | ✅ (but works on any wlroots compositor) |
| **Daemon** | ✅ (`hyprpaper`) | Optional systemd service |
| **Preloading** | ✅ | ⚠️ TODO (coming soon) |
| **Transitions** | ❌ | ✅ |
| **Scaling** | Basic | Full (fill, stretch, original, panscan) |
| **IPC Control** | ❌ | ✅ |
| **Multi-Monitor** | ✅ | ✅ |

### hyprpaper Commands → gSlapper Equivalents

#### Basic Usage

**hyprpaper:**
```bash
# Start daemon
hyprpaper &

# Preload images
hyprpaper preload /path/to/image1.jpg
hyprpaper preload /path/to/image2.jpg

# Set wallpaper
hyprpaper wallpaper "DP-1,/path/to/image1.jpg"
hyprpaper wallpaper "DP-2,/path/to/image2.jpg"
```

**gSlapper:**
```bash
# Set wallpaper directly (no daemon required)
gslapper -o "fill" DP-1 /path/to/image1.jpg

# For multiple monitors (different wallpapers)
gslapper -o "fill" DP-1 /path/to/image1.jpg &
gslapper -o "fill" DP-2 /path/to/image2.jpg &

# Or use systemd service for persistence
# See: Persistent Wallpapers guide
```

#### Different Wallpapers per Monitor

**hyprpaper (`~/.config/hypr/hyprpaper.conf`):**
```bash
wallpaper = DP-1,/path/to/image1.jpg
wallpaper = DP-2,/path/to/image2.jpg
```

**gSlapper (systemd template services):**
```bash
# Enable per-monitor services
systemctl --user enable --now gslapper@DP-1.service
systemctl --user enable --now gslapper@DP-2.service

# Each service has its own state file
# Set wallpapers manually first
gslapper -o "fill" DP-1 /path/to/image1.jpg  # Ctrl+C
gslapper -o "fill" DP-2 /path/to/image2.jpg  # Ctrl+C
```

### hyprpaper Config → gSlapper

**hyprpaper config (`~/.config/hypr/hyprpaper.conf`):**
```bash
wallpaper = DP-1,/path/to/wallpaper.jpg
wallpaper = DP-2,/path/to/wallpaper2.jpg
splash = false
```

**gSlapper equivalent (Hyprland config):**

**Method 1: Compositor startup (quick setup)**
```bash
# ~/.config/hypr/hyprland.conf
exec-once = gslapper -o "fill" DP-1 /path/to/wallpaper.jpg
```

**Method 2: Systemd service (recommended)**
```bash
# Create environment
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" > ~/.config/gslapper/environment

# Enable services
systemctl --user enable --now gslapper@DP-1.service
systemctl --user enable --now gslapper@DP-2.service
```

**Method 3: Shell script (for manual control)**
```bash
#!/bin/bash
# ~/.config/hypr/set-wallpapers.sh
gslapper -o "fill" DP-1 /path/to/wallpaper.jpg &
gslapper -o "fill" DP-2 /path/to/wallpaper2.jpg &
```

### Migration Checklist - hyprpaper Users

- [ ] Kill `hyprpaper` daemon:
  ```bash
  pkill hyprpaper
  ```
- [ ] Remove from Hyprland config:
  ```bash
  # Comment out in ~/.config/hypr/hyprland.conf
  # exec-once = hyprpaper &
  ```
- [ ] Set up gSlapper via systemd (recommended) or compositor config
- [ ] Test with static images first
- [ ] Try video wallpapers (new feature not available in hyprpaper)
- [ ] Try fade transitions
- [ ] Remove hyprpaper config file if not needed

---

## Migrating from swaybg

### swaybg vs gSlapper Comparison

| Feature | swaybg | gSlapper |
|---------|---------|-----------|
| **Static Images** | ✅ | ✅ |
| **Video Wallpapers** | ❌ | ✅ |
| **Sway Only** | ✅ | ✅ (but works on any Wayland compositor) |
| **Daemon** | ✅ (runs in bg) | Optional systemd service |
| **Transitions** | ❌ | ✅ |
| **Scaling** | `stretch`/`fill` only | Full modes |
| **IPC Control** | ❌ | ✅ |
| **Multi-Monitor** | ✅ | ✅ |

### swaybg Commands → gSlapper Equivalents

#### Basic Usage

**swaybg:**
```bash
# Single monitor
swaybg -i /path/to/image.jpg -m fill

# Multiple monitors (different wallpapers)
swaybg -i /path/to/image1.jpg -o DP-1 -m fill \
         -i /path/to/image2.jpg -o DP-2 -m fill

# Scale modes
swaybg -i /path/to/image.jpg -m stretch
swaybg -i /path/to/image.jpg -m fit
```

**gSlapper:**
```bash
# Single monitor
gslapper -o "fill" DP-1 /path/to/image.jpg

# Multiple monitors
gslapper -o "fill" DP-1 /path/to/image1.jpg &
gslapper -o "fill" DP-2 /path/to/image2.jpg &

# Scale modes
gslapper -o "stretch" DP-1 /path/to/image.jpg
gslapper -o "original" DP-1 /path/to/image.jpg
gslapper -o "panscan=1.0" DP-1 /path/to/image.jpg  # Fit
```

#### Sway Config → gSlapper

**swaybg in Sway config (`~/.config/sway/config`):**
```bash
# Single wallpaper
output * bg /path/to/wallpaper.jpg fill

# Different per monitor
output DP-1 bg /path/to/wallpaper1.jpg fill
output DP-2 bg /path/to/wallpaper2.jpg fill

# Scale modes
output DP-1 bg /path/to/wallpaper.jpg stretch
output DP-1 bg /path/to/wallpaper.jpg fit
```

**gSlapper equivalent:**

**Method 1: Exec in sway config**
```bash
# Single wallpaper
exec gslapper -o "fill" '*' /path/to/wallpaper.jpg

# Different per monitor (script)
exec ~/.config/sway/set-wallpapers.sh
```

**Method 2: Systemd service (recommended)**
```bash
# Create environment
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" > ~/.config/gslapper/environment

# Enable services
systemctl --user enable --now gslapper.service  # Single
# Or per monitor:
systemctl --user enable --now gslapper@DP-1.service
systemctl --user enable --now gslapper@DP-2.service
```

**Method 3: Script for sway exec**
```bash
#!/bin/bash
# ~/.config/sway/set-wallpapers.sh
gslapper -o "fill" DP-1 /path/to/wallpaper1.jpg &
gslapper -o "fill" DP-2 /path/to/wallpaper2.jpg &
```

### Migration Checklist - swaybg Users

- [ ] Remove `output * bg` lines from sway config:
  ```bash
  # Comment out in ~/.config/sway/config
  # output * bg /path/to/wallpaper.jpg fill
  ```
- [ ] Kill any running swaybg instances:
  ```bash
  pkill swaybg
  ```
- [ ] Set up gSlapper via systemd (recommended) or sway exec
- [ ] Test static image wallpaper
- [ ] Try video wallpapers (new feature)
- [ ] Try fade transitions
- [ ] Enable IPC socket if you want runtime control

---

## Migrating from feh

### feh vs gSlapper Comparison

| Feature | feh | gSlapper |
|---------|------|-----------|
| **Static Images** | ✅ | ✅ |
| **Video Wallpapers** | ❌ | ✅ |
| **X11 Tool** | ✅ | ❌ (Wayland only) |
| **Xwayland Compatible** | ✅ | ✅ (native Wayland better) |
| **Image Browser** | ✅ | ❌ |
| **Transitions** | ❌ | ✅ |
| **Scaling** | Full modes | Full modes |
| **IPC Control** | ❌ | ✅ |
| **Multi-Monitor** | ✅ | ✅ |

**Note**: feh is an X11 tool. If you're using Wayland with Xwayland, gSlapper (native Wayland) will provide better performance and integration.

### feh Commands → gSlapper Equivalents

#### Basic Usage

**feh:**
```bash
# Set wallpaper
feh --bg-fill /path/to/image.jpg

# Specific monitor (with xrandr)
feh --bg-fill /path/to/image.jpg --output-name DP-1

# Scale modes
feh --bg-scale /path/to/image.jpg    # Fit inside
feh --bg-tile /path/to/image.jpg     # Tile
feh --bg-center /path/to/image.jpg   # Center
```

**gSlapper:**
```bash
# Set wallpaper
gslapper -o "fill" DP-1 /path/to/image.jpg

# Scale modes
gslapper -o "panscan=1.0" DP-1 /path/to/image.jpg  # Fit
gslapper -o "original" DP-1 /path/to/image.jpg     # Center/native
```

#### feh Random Wallpaper Scripts

**feh script:**
```bash
#!/bin/bash
# feh random wallpaper
DIR="$HOME/Pictures/Wallpapers"
IMAGE=$(find "$DIR" -type f | shuf -n 1)
feh --bg-fill "$IMAGE"
```

**gSlapper equivalent:**
```bash
#!/bin/bash
# gSlapper random wallpaper
DIR="$HOME/Pictures/Wallpapers"
IMAGE=$(find "$DIR" -type f | shuf -n 1)
gslapper -o "fill" '*' "$IMAGE"
```

### feh Integration in Compositors

**Sway with feh:**
```bash
# ~/.config/sway/config
exec feh --bg-fill /path/to/wallpaper.jpg
```

**gSlapper equivalent:**
```bash
# ~/.config/sway/config
exec gslapper -o "fill" '*' /path/to/wallpaper.jpg
```

### Migration Checklist - feh Users

- [ ] Ensure you're using Wayland (native Wayland is better than Xwayland)
- [ ] Remove feh from compositor config
- [ ] Kill any running feh instances:
  ```bash
  pkill feh
  ```
- [ ] Set up gSlapper (see tool-specific guides above)
- [ ] Test with static images
- [ ] Try video wallpapers (new feature)
- [ ] Try fade transitions
- [ ] Keep feh for image browsing if you like it (both can coexist)

---

## Quick Reference: Common Commands

### Setting Static Wallpaper

| Tool | Command |
|-------|---------|
| swww | `swww img /path/to/image.jpg` |
| mpvpaper | `mpvpaper DP-1 /path/to/image.jpg` |
| hyprpaper | `hyprpaper wallpaper "DP-1,/path/to/image.jpg"` |
| swaybg | `swaybg -i /path/to/image.jpg -m fill -o DP-1` |
| feh | `feh --bg-fill /path/to/image.jpg` |
| **gSlapper** | `gslapper -o "fill" DP-1 /path/to/image.jpg` |

### Setting Video Wallpaper

| Tool | Command |
|-------|---------|
| swww | ❌ Not supported |
| mpvpaper | `mpvpaper -f --loop DP-1 /path/to/video.mp4` |
| hyprpaper | ❌ Not supported |
| swaybg | ❌ Not supported |
| feh | ❌ Not supported |
| **gSlapper** | `gslapper -o "loop" DP-1 /path/to/video.mp4` |

### Fade Transition

| Tool | Command |
|-------|---------|
| swww | `swww img -t fade /path/to/image.jpg` |
| mpvpaper | ❌ Not supported |
| hyprpaper | ❌ Not supported |
| swaybg | ❌ Not supported |
| feh | ❌ Not supported |
| **gSlapper** | `gslapper --transition-type fade --transition-duration 0.5 DP-1 /path/to/image.jpg` |

---

## Troubleshooting Migration Issues

### Compositor Still Starts Old Tool

**Problem**: Old wallpaper tool still runs on startup

**Solution**:
1. Check compositor config files:
   - Hyprland: `~/.config/hypr/hyprland.conf`
   - Sway: `~/.config/sway/config`
   - River: `~/.config/river/init`
   - Niri: `~/.config/niri/config.kdl`

2. Remove or comment out old tool:
   ```bash
   # Old
   # exec-once = hyprpaper &

   # New
   exec-once = gslapper -o "fill" '*' /path/to/wallpaper.jpg
   ```

3. Kill running instances:
   ```bash
   pkill hyprpaper
   pkill swaybg
   pkill swww-daemon
   ```

### Multiple Wallpaper Tools Running

**Problem**: Conflicts between multiple wallpaper tools

**Solution**:
```bash
# Check for running tools
ps aux | grep -E "swww|mpvpaper|hyprpaper|swaybg|feh|gslapper"

# Kill old tools
systemctl --user stop swww-daemon.service
pkill -f "mpvpaper|hyprpaper|swaybg|feh"

# Start gSlapper
gslapper -o "fill" '*' /path/to/wallpaper.jpg
```

### Wallpaper Not Showing

**Problem**: gSlapper starts but no wallpaper visible

**Solutions**:
1. Check output name:
   ```bash
   gslapper -d  # List available outputs
   ```

2. Check if file exists and is readable:
   ```bash
   test -f /path/to/wallpaper.jpg && echo "OK" || echo "Missing"
   ```

3. Check logs:
   ```bash
   # With verbose flag
   gslapper -vv DP-1 /path/to/wallpaper.jpg

   # Systemd logs
   journalctl --user -u gslapper.service -f
   ```

### Transition Not Working

**Problem**: Fade transition doesn't appear

**Cause**: Transitions only work between static images, not videos

**Solution**:
```bash
# Correct - static image to static image
gslapper -o "fill" --transition-type fade DP-1 image1.jpg
# Then:
echo "change /path/to/image2.jpg" | nc -U /tmp/gslapper.sock

# Incorrect - video wallpaper
gslapper -o "loop" DP-1 video.mp4  # No transition
```

---

## Additional Resources

- **[Quick Start Guide](./getting-started/quick-start.md)** - Get started with gSlapper
- **[Command Line Options](./user-guide/command-line-options.md)** - Full CLI reference
- **[Systemd Service Setup](./systemd-service-setup.md)** - Persistent wallpapers guide
- **[Persistent Wallpapers](./user-guide/persistent-wallpapers.md)** - Advanced persistence methods
- **[IPC Control](./user-guide/ipc-control.md)** - Runtime control guide
- **[Troubleshooting](./advanced/troubleshooting.md)** - Common issues and solutions

---

## Feature Comparison Summary

| Feature | swww | mpvpaper | hyprpaper | swaybg | feh | gSlapper |
|---------|-------|-----------|------------|---------|------|-----------|
| **Static Images** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Video Wallpapers** | ❌ | ✅ | ❌ | ❌ | ❌ | ✅ |
| **Transitions** | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| **IPC Control** | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| **Preload Cache** | ❌ | ❌ | ✅ | ❌ | ❌ | ⚠️ TODO |
| **Persistent State** | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| **NVIDIA Friendly** | ✅ | ⚠️ | ✅ | ✅ | ✅ | ✅ |
| **GPU Optimized** | Medium | High | Low | Low | Low | **Low** |
| **Backend** | Custom | libmpv | Custom | Custom | X11 | GStreamer |
| **Multi-Monitor** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

**gSlapper combines the best features:**
- Static images + videos (like mpvpaper)
- Transitions (like swww)
- IPC control (like swww)
- Low GPU usage (better than mpvpaper)
- No NVIDIA memory leaks (unlike mpvpaper)
- Wayland native (unlike feh)
