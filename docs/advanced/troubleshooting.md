# Troubleshooting

Common issues and their solutions.

## Video Won't Play

### "Pipeline failed to reach playing state"

**Cause**: Missing codec support

**Solution**: Install GStreamer codec plugins:

```bash
# Arch Linux
sudo pacman -S gst-plugins-ugly gst-libav

# Ubuntu/Debian
sudo apt install gstreamer1.0-plugins-ugly gstreamer1.0-libav
```

### Video Plays But No Display

**Cause**: Wayland compositor not detected or EGL initialization failed

**Solution**:
1. Verify you're running a Wayland compositor (Hyprland, Sway, etc.)
2. Check EGL support: `gst-inspect-1.0 egl`
3. Try with verbose output: `gslapper -vv DP-1 video.mp4`

### Video Plays But Freezes

**Cause**: GStreamer pipeline issues or resource exhaustion

**Solution**:
1. Check system resources: `htop`
2. Restart gSlapper
3. Try a different video file
4. Check logs with `-vv` flag

## Image Issues

### Image Not Displaying

**Cause**: Unsupported format or file corruption

**Solution**:
1. Verify file format is supported (JPEG, PNG, WebP, GIF)
2. Check file is not corrupted: `file /path/to/image.jpg`
3. Try with another image
4. Use verbose mode: `gslapper -vv DP-1 image.jpg`

### Image Shows Letterboxing

**Cause**: Wrong scaling mode

**Solution**: Use fill mode:
```bash
gslapper -o "fill" DP-1 image.jpg
```

### Image Looks Distorted

**Cause**: Stretch mode enabled

**Solution**: Use fill mode instead:
```bash
gslapper -o "fill" DP-1 image.jpg
```

## Transition Issues

### Transitions Not Working

**Cause**: IPC not enabled or transitions disabled

**Solution**:
1. Enable IPC: `-I /tmp/gslapper.sock`
2. Enable transitions: `--transition-type fade`
3. Verify both are set:
```bash
gslapper --transition-type fade --transition-duration 2.0 -I /tmp/gslapper.sock DP-1 image.jpg
```

### Transition Shows Letterboxing

**Cause**: This was a bug that has been fixed in recent versions

**Solution**: Update to latest version. Both images should fill the screen correctly.

### Transition Too Fast/Slow

**Cause**: Duration not set correctly

**Solution**: Adjust duration:
```bash
# Faster (0.5 seconds)
gslapper --transition-type fade --transition-duration 0.5 -I /tmp/sock DP-1 image.jpg

# Slower (3.0 seconds)
gslapper --transition-type fade --transition-duration 3.0 -I /tmp/sock DP-1 image.jpg
```

## IPC Issues

### "Connection refused" or "No such file"

**Cause**: IPC socket not created or gSlapper not running

**Solution**:
1. Verify gSlapper is running: `pgrep gslapper`
2. Check socket exists: `ls -l /tmp/gslapper.sock`
3. Ensure IPC was enabled: `gslapper -I /tmp/gslapper.sock ...`

### IPC Commands Not Working

**Cause**: Wrong socket path or permission issues

**Solution**:
1. Use correct socket path
2. Check permissions: `ls -l /tmp/gslapper.sock`
3. Try with socat instead of nc:
```bash
echo "query" | socat - UNIX-CONNECT:/tmp/gslapper.sock
```

### List Available Commands

Use the `help` command to see all available IPC commands:

```bash
echo "help" | nc -U /tmp/gslapper.sock
```

### Understanding Error Messages

Recent versions provide detailed error messages:

| Error Message | Meaning |
|---------------|---------|
| `File not accessible: <path> (No such file or directory)` | File doesn't exist |
| `File not accessible: <path> (Permission denied)` | Insufficient permissions |
| `Path too long (max 4096 characters)` | File path exceeds maximum length |
| `Invalid input: contains control characters` | Input contains forbidden characters |
| `Unknown command` | Use `help` to list valid commands |

## Multi-Monitor Issues

### Wrong Monitor Selected

**Cause**: Monitor name mismatch

**Solution**:
1. List available monitors: `gslapper -d`
2. Use exact name or identifier
3. Try with `'*'` for all monitors

### Wallpaper Not Showing on All Monitors

**Cause**: Multiple instances interfering

**Solution**:
1. Kill all instances: `pkill gslapper`
2. Start fresh with `'*'`:
```bash
gslapper -o "loop" '*' video.mp4
```

## Performance Issues

### High CPU Usage

See [Performance Tuning](performance.md) for detailed solutions.

Quick fixes:
- Lower FPS: `--fps-cap 30`
- Use auto-pause: `-p`
- Use static images instead of videos

### High Memory Usage

**Cause**: Large video files or memory leak

**Solution**:
1. Use lower resolution videos
2. Restart gSlapper periodically
3. Check for updates (memory leaks are fixed in newer versions)

## Build Issues

### "meson: command not found"

**Solution**: Install build dependencies:
```bash
# Arch Linux
sudo pacman -S meson ninja

# Ubuntu/Debian
sudo apt install meson ninja-build
```

### "wayland-protocols not found"

**Solution**: Install Wayland development packages:
```bash
# Arch Linux
sudo pacman -S wayland-protocols

# Ubuntu/Debian
sudo apt install wayland-protocols
```

### Compilation Errors

**Solution**:
1. Ensure all dependencies are installed
2. Clean build directory: `rm -rf build && meson setup build`
3. Check for error messages in build output

## Getting Help

### Verbose Output

Always include verbose output when reporting issues:

```bash
gslapper -vv DP-1 video.mp4 > /tmp/gslapper.log 2>&1
```

### System Information

Include when reporting issues:
- Distribution and version
- Wayland compositor (Hyprland, Sway, etc.)
- GPU and drivers
- GStreamer version: `gst-launch-1.0 --version`

### Reporting Issues

1. Check existing issues on GitHub
2. Include verbose log output
3. Include system information
4. Describe steps to reproduce
