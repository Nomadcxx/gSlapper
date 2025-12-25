# Systemd Service Setup

## Overview

gSlapper can run as a systemd user service for automatic wallpaper management at login.

## Installation

The service is installed automatically via the AUR package to:
```
/usr/lib/systemd/user/gslapper.service
```

## Configuration

### 1. Create Environment File

**Required:** Create `~/.config/gslapper/environment` with your Wayland display socket:

```bash
mkdir -p ~/.config/gslapper
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" > ~/.config/gslapper/environment
```

**Finding your WAYLAND_DISPLAY:**

```bash
# In your Wayland session, run:
echo $WAYLAND_DISPLAY
# Common values: wayland-0, wayland-1
```

**Example environment file:**

```bash
# ~/.config/gslapper/environment
WAYLAND_DISPLAY=wayland-1
```

### 2. Set Initial Wallpaper

Start gSlapper manually once to set your wallpaper:

```bash
gslapper -o "loop" DP-1 /path/to/video.mp4
# or
gslapper -o "fill" DP-1 /path/to/image.jpg
```

Stop it with Ctrl+C. The state is saved automatically.

## Usage

### Enable and Start Service

```bash
# Enable service to start at login
systemctl --user enable gslapper.service

# Start service now
systemctl --user start gslapper.service

# Check status
systemctl --user status gslapper.service
```

### Service Commands

```bash
# Reload (saves state, restarts)
systemctl --user reload gslapper.service

# Restart (saves state, restarts)
systemctl --user restart gslapper.service

# Stop
systemctl --user stop gslapper.service

# Disable autostart
systemctl --user disable gslapper.service

# View logs
journalctl --user -u gslapper.service -f
```

## How It Works

1. **Service starts** with `--systemd --restore` flags
2. **Restores state** from `~/.local/state/gslapper/<output>.state.txt`
3. **Resumes wallpaper** at saved position (videos) or shows saved image
4. **On reload/stop** saves current state automatically
5. **On restart** restores from saved state

## State Management

### State Files

Per-monitor state files are saved to:
```
~/.local/state/gslapper/DP-1.state.txt
~/.local/state/gslapper/DP-3.state.txt
```

### State Contents

```
version=1
output=DP-1
path=/home/user/Videos/wallpaper.mp4
is_image=false
options=loop
position=45.230000
paused=false
```

### Manual State Management

```bash
# Save current state manually
gslapper --save-state

# Restore from state
gslapper --restore

# Use custom state file
gslapper --state-file /tmp/my-state.txt --restore

# Disable auto-save on exit
gslapper --no-save-state
```

## Troubleshooting

### Service fails to start

**Check logs:**
```bash
journalctl --user -u gslapper.service -n 50
```

**Common issues:**

1. **Missing environment file:**
   ```
   Error: Unable to connect to compositor
   ```
   Solution: Create `~/.config/gslapper/environment` with correct `WAYLAND_DISPLAY`

2. **Wrong WAYLAND_DISPLAY:**
   ```
   Error: Unable to connect to compositor
   ```
   Solution: Check `echo $WAYLAND_DISPLAY` in your session and update environment file

3. **No state file:**
   Service uses `--restore` but no state exists - it will fail gracefully and exit

### Service starts but no wallpaper

- Verify state file exists: `ls -la ~/.local/state/gslapper/`
- Check state file contents: `cat ~/.local/state/gslapper/<output>.state.txt`
- Verify video/image path in state file still exists
- Check service logs for errors

### State not persisting

- Verify directory is writable: `ls -ld ~/.local/state/gslapper/`
- Check for disk space: `df -h ~`
- Review logs for save errors: `journalctl --user -u gslapper.service | grep state`

## Advanced Configuration

### Resource Limits

The service has default resource limits:
- Memory: 512MB max
- CPU: 50% of one core

To customize, create override:

```bash
systemctl --user edit gslapper.service
```

Add:
```ini
[Service]
MemoryMax=1G
CPUQuota=100%
```

### Security Hardening

Default security settings:
- `NoNewPrivileges=true` - Prevents privilege escalation
- `PrivateTmp=true` - Private /tmp directory

### Multiple Monitors

To run separate instances per monitor:

```bash
# Disable the main service
systemctl --user disable gslapper.service

# Create instance services
cp /usr/lib/systemd/user/gslapper.service ~/.config/systemd/user/gslapper@.service
```

Edit `gslapper@.service`:
```ini
ExecStart=/usr/bin/gslapper --systemd --restore --state-file %h/.local/state/gslapper/%i.state.txt
```

Enable per monitor:
```bash
systemctl --user enable gslapper@DP-1.service
systemctl --user enable gslapper@DP-3.service
```

## Integration with IPC

The service can be controlled via IPC socket:

```bash
# Start service with IPC
# (Edit service to add --ipc-socket flag)
systemctl --user edit gslapper.service
```

Add:
```ini
[Service]
ExecStart=
ExecStart=/usr/bin/gslapper --systemd --restore --ipc-socket /run/user/%U/gslapper.sock
```

Reload and use:
```bash
systemctl --user daemon-reload
systemctl --user restart gslapper.service

# Control via IPC
echo "pause" | nc -U /run/user/$UID/gslapper.sock
echo "resume" | nc -U /run/user/$UID/gslapper.sock
```
