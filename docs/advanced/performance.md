# Performance Tuning

gSlapper is designed to be efficient, but there are several options to optimize performance for your system.

## Frame Rate Control

### Frame Rate Cap

Control the maximum frame rate to reduce GPU/CPU usage:

```bash
gslapper --fps-cap 30 -o "loop" DP-1 video.mp4
```

Available options:
- `30` - 30 FPS (default, good balance)
- `60` - 60 FPS (smoother, more resource intensive)
- `100` - 100 FPS (maximum smoothness)

### When to Adjust

- **Lower FPS (30)**: Older hardware, battery-powered devices, multiple monitors
- **Higher FPS (60-100)**: High-end GPUs, single monitor, smooth video playback

## Auto-Pause/Stop

Reduce resource usage when wallpapers are hidden:

### Auto-Pause

Pauses playback when certain applications are fullscreen:

```bash
gslapper -p -o "loop" DP-1 video.mp4
```

Configure apps in `~/.config/mpvpaper/pauselist`:
```
firefox
chromium
mpv
vlc
```

### Auto-Stop

Completely stops playback when wallpaper is hidden:

```bash
gslapper -s -o "loop" DP-1 video.mp4
```

Configure apps in `~/.config/mpvpaper/stoplist`:
```
games
```

## Video Optimization

### Codec Selection

Use hardware-accelerated codecs when available:

- **H.264** - Widely supported, good performance
- **H.265/HEVC** - Better compression, requires more processing
- **VP9** - Open format, good compression

### Video Resolution

Lower resolution videos use fewer resources:

```bash
# Use 1080p instead of 4K for better performance
gslapper -o "loop" DP-1 video_1080p.mp4
```

## Image Optimization

### Static Images

Static images are very efficient. For best performance:

- Use appropriate resolution (don't use 8K images on 1080p displays)
- Prefer JPEG for photos (smaller file size)
- Use PNG only when transparency is needed

### Transitions

Transitions add some CPU overhead:

- Shorter durations (0.5-1.0s) use fewer resources
- Longer durations (3.0s+) are smoother but use more CPU
- Disable transitions if performance is critical

## Multi-Monitor Performance

Running multiple instances uses more resources:

### Tips

1. **Mix static and video** - Use static images on secondary monitors
2. **Lower FPS on secondary** - Use `--fps-cap 30` on non-primary monitors
3. **Use auto-pause** - Pause wallpapers when not visible

```bash
# Primary monitor: 60 FPS video
gslapper --fps-cap 60 -o "loop" DP-1 video.mp4 &

# Secondary monitor: 30 FPS video
gslapper --fps-cap 30 -o "loop" HDMI-1 video2.mp4 &

# Tertiary monitor: Static image (most efficient)
gslapper -o "fill" eDP-1 image.jpg &
```

## GPU Acceleration

gSlapper uses GStreamer's hardware acceleration when available:

### NVIDIA

Ensure proper drivers and GStreamer plugins:
```bash
# Arch Linux
sudo pacman -S gst-plugins-bad gst-plugins-ugly

# Verify hardware acceleration
gst-inspect-1.0 nvdec
```

### AMD/Intel

Hardware acceleration is typically automatic with Mesa drivers.

## Memory Usage

### Video Wallpapers

- Memory usage depends on video resolution and codec
- 1080p videos: ~50-100 MB
- 4K videos: ~200-400 MB

### Static Images

- Very low memory usage
- Typically < 50 MB regardless of image size

## Monitoring Performance

### Verbose Output

Use verbose mode to see performance information:

```bash
gslapper -vv -o "loop" DP-1 video.mp4
```

Look for:
- Frame processing times
- Texture allocation messages
- Pipeline state changes

### System Monitoring

Monitor resource usage:

```bash
# CPU and memory
htop

# GPU usage (NVIDIA)
nvidia-smi -l 1

# GPU usage (AMD/Intel)
radeontop  # or intel_gpu_top
```

## Troubleshooting Performance Issues

### High CPU Usage

1. Lower frame rate cap: `--fps-cap 30`
2. Use auto-pause: `-p`
3. Reduce video resolution
4. Disable transitions

### High Memory Usage

1. Use static images instead of videos
2. Lower video resolution
3. Check for memory leaks (restart periodically)

### Stuttering/Frame Drops

1. Lower frame rate cap
2. Check GPU drivers are up to date
3. Ensure hardware acceleration is working
4. Close other GPU-intensive applications
