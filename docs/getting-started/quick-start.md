# Quick Start

Get gSlapper running in under a minute!

## Basic Video Wallpaper

```bash
gslapper DP-1 /path/to/video.mp4
```

Replace `DP-1` with your monitor name (use `gslapper -d` to list available monitors).

## Basic Image Wallpaper

```bash
gslapper DP-1 /path/to/wallpaper.jpg
```

Images default to "fill" mode, which fills the screen while maintaining aspect ratio.

## With Looping

```bash
gslapper -o "loop" DP-1 /path/to/video.mp4
```

## All Monitors

```bash
gslapper -o "loop" '*' /path/to/video.mp4
```

## Background Mode

Run in the background:

```bash
gslapper -f -o "loop" DP-1 /path/to/video.mp4
```

## Making Wallpapers Persistent

To make your wallpaper survive reboots and logins, see the [Persistent Wallpapers](../user-guide/persistent-wallpapers.md) guide. It covers:
- Systemd service setup (recommended)
- Shell script startup
- Compositor-specific configuration
- Manual restoration

## Next Steps

- Learn about [Video Wallpapers](../user-guide/video-wallpapers.md)
- Explore [Static Images](../user-guide/static-images.md)
- Set up [Persistent Wallpapers](../user-guide/persistent-wallpapers.md) for automatic restoration
- Set up [IPC Control](../user-guide/ipc-control.md) for runtime control
- Check [Scaling Modes](../user-guide/scaling-modes.md) for display options
