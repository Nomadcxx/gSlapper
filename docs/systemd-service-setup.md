# Systemd Service Setup for gSlapper

gSlapper can run as a systemd user service, similar to how `swww-daemon` works. This allows automatic wallpaper restoration on login.

## Quick Setup

### 1. Create Environment File

**Required:** Create `~/.config/gslapper/environment` with your Wayland display:

```bash
mkdir -p ~/.config/gslapper
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" > ~/.config/gslapper/environment
```

**Finding your WAYLAND_DISPLAY:**
```bash
echo $WAYLAND_DISPLAY
# Common values: wayland-0, wayland-1
```

### 2. Set Initial Wallpaper

Start gSlapper manually once to set your wallpaper (this saves state):

```bash
# Single monitor
gslapper -o "loop" DP-1 /path/to/video.mp4

# All monitors (same wallpaper)
gslapper -o "loop" '*' /path/to/video.mp4
```

Stop with `Ctrl+C`. The state is saved automatically.

### 3. Create Service File

Choose one of the following approaches:

#### Option A: Single Service (All Monitors - Same Wallpaper)

Create `~/.config/systemd/user/gslapper.service`:

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

# CRITICAL: Wayland environment variables
EnvironmentFile=%h/.config/gslapper/environment
Environment=XDG_RUNTIME_DIR=%t

# Resource limits
MemoryMax=512M
CPUQuota=50%

# Security
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=default.target
```

**Enable and start:**
```bash
systemctl --user daemon-reload
systemctl --user enable --now gslapper.service
```

#### Option B: Per-Monitor Services (Different Wallpapers)

Create `~/.config/systemd/user/gslapper@.service`:

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

# CRITICAL: Wayland environment variables
EnvironmentFile=%h/.config/gslapper/environment
Environment=XDG_RUNTIME_DIR=%t

# Resource limits
MemoryMax=512M
CPUQuota=50%

# Security
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=default.target
```

**Set wallpapers per monitor:**
```bash
# Set wallpaper on DP-1
gslapper -o "loop" DP-1 /path/to/video1.mp4
# Ctrl+C to stop (saves state)

# Set wallpaper on DP-3
gslapper -o "fill" DP-3 /path/to/image.jpg
# Ctrl+C to stop (saves state)
```

**Enable per monitor:**
```bash
systemctl --user daemon-reload
systemctl --user enable --now gslapper@DP-1.service
systemctl --user enable --now gslapper@DP-3.service
```

## Service Management

```bash
# Check status
systemctl --user status gslapper.service

# View logs
journalctl --user -u gslapper.service -f

# Reload (saves state, restarts)
systemctl --user reload gslapper.service

# Restart
systemctl --user restart gslapper.service

# Stop
systemctl --user stop gslapper.service

# Disable autostart
systemctl --user disable gslapper.service
```

## How It Works

1. **Service starts** with `--systemd --restore <output>` flags
2. **Restores state** from `~/.local/state/gslapper/state-<output>.txt`
3. **Resumes wallpaper** at saved position (videos) or shows saved image
4. **On reload/stop** saves current state automatically
5. **On restart** restores from saved state

## State Files

State files are saved automatically to:
```
~/.local/state/gslapper/state-DP-1.txt
~/.local/state/gslapper/state-DP-3.txt
~/.local/state/gslapper/state.txt  (for '*' output)
```

Each file contains:
```
version=1
output=DP-1
path=/path/to/wallpaper.mp4
type=video
options=loop panscan=0.8
position=123.45
paused=0
```

## Comparison with swww

**swww:**
- `swww-daemon` runs as a single daemon process
- Systemd service: `ExecStart=/usr/bin/swww-daemon`
- Automatic cache restoration on startup (built-in)
- Manages all outputs in one process

**gSlapper:**
- One process per wallpaper (like mpvpaper)
- Systemd service: `ExecStart=/usr/bin/gslapper --systemd --restore <output>`
- Explicit state restoration via `--restore` flag
- Each monitor can have its own service instance
- More detailed state (video position, pause state, options)

**Why the difference:**
- swww is a daemon that stays alive - automatic cache restoration works naturally
- gSlapper is one-shot process - needs systemd to restart and restore after reboot
- Both approaches work, but gSlapper's systemd service is essential for persistence

## Troubleshooting

**Service fails to start:**
- Check `journalctl --user -u gslapper.service`
- Verify `~/.config/gslapper/environment` exists and has `WAYLAND_DISPLAY`
- Ensure state file exists (set wallpaper manually first)

**Wallpaper not restoring:**
- Check state file exists: `ls ~/.local/state/gslapper/`
- Verify path in state file still exists
- Check service logs for errors

**Multiple monitors:**
- Use template service (`gslapper@.service`) for per-monitor instances
- Or use single service with `'*'` for same wallpaper on all monitors
