# Multi-Monitor Setup

gSlapper provides full support for multiple displays, allowing you to set different wallpapers on each monitor or use the same wallpaper across all displays.

## Listing Available Monitors

Use the `-d` or `--help-output` option to list all available monitors:

```bash
gslapper -d
```

Output example:
```
[*] Output: eDP-1  Identifier: InfoVision Optoelectronics
[*] Output: DP-1   Identifier: Dell Inc. DELL U2718Q
[*] Output: HDMI-1 Identifier: Samsung Electronics
```

## Single Monitor

Set wallpaper on a specific monitor:

```bash
# By name
gslapper DP-1 /path/to/video.mp4

# By identifier (if name doesn't work)
gslapper "Dell Inc. DELL U2718Q" /path/to/video.mp4
```

## All Monitors

Use `'*'` to set the same wallpaper on all monitors:

```bash
gslapper -o "loop" '*' /path/to/video.mp4
```

## Different Wallpapers per Monitor

Run multiple instances, one per monitor:

```bash
# Monitor 1
gslapper -f -o "loop" DP-1 /path/to/video1.mp4 &

# Monitor 2
gslapper -f -o "loop" HDMI-1 /path/to/video2.mp4 &

# Monitor 3
gslapper -f -o "loop" eDP-1 /path/to/image.jpg &
```

## IPC Control per Monitor

Each monitor can have its own IPC socket:

```bash
# Monitor 1 with IPC
gslapper -I /tmp/gslapper-dp1.sock -o "loop" DP-1 video1.mp4 &

# Monitor 2 with IPC
gslapper -I /tmp/gslapper-hdmi1.sock -o "loop" HDMI-1 video2.mp4 &

# Control each independently
echo "pause" | nc -U /tmp/gslapper-dp1.sock
echo "change /path/to/new.mp4" | nc -U /tmp/gslapper-hdmi1.sock
```

## Display Scaling

gSlapper automatically handles display scaling. Each monitor's scale factor is detected and applied correctly.

## Best Practices

1. **Use background mode** - Add `-f` flag when running multiple instances
2. **Unique IPC sockets** - Use different socket paths for each monitor
3. **Resource management** - Multiple video wallpapers use more GPU/CPU resources
4. **Startup scripts** - Use systemd user services or shell scripts to manage multiple instances

## Example Startup Script

```bash
#!/bin/bash
# ~/.local/bin/start-wallpapers.sh

# Kill existing instances
pkill gslapper

# Wait a moment
sleep 1

# Start wallpapers for each monitor
gslapper -f -I /tmp/gslapper-dp1.sock -o "loop" DP-1 ~/Videos/wallpaper1.mp4 &
sleep 0.5

gslapper -f -I /tmp/gslapper-hdmi1.sock -o "loop" HDMI-1 ~/Videos/wallpaper2.mp4 &
sleep 0.5

gslapper -f -I /tmp/gslapper-edp1.sock -o "fill" eDP-1 ~/Pictures/wallpaper.jpg &
```

## Systemd User Service

Create `~/.config/systemd/user/wallpapers.service`:

```ini
[Unit]
Description=Wallpapers for all monitors
After=graphical-session.target

[Service]
Type=forking
ExecStart=/home/user/.local/bin/start-wallpapers.sh
ExecStop=/usr/bin/pkill gslapper
Restart=on-failure

[Install]
WantedBy=default.target
```

Enable and start:
```bash
systemctl --user enable wallpapers.service
systemctl --user start wallpapers.service
```
