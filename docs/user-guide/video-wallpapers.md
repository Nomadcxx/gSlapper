# Video Wallpapers

gSlapper supports all video formats that GStreamer can decode, including MP4, MKV, WebM, AVI, MOV, and more.

## Basic Usage

```bash
gslapper DP-1 /path/to/video.mp4
```

## Looping

To loop a video seamlessly:

```bash
gslapper -o "loop" DP-1 /path/to/video.mp4
```

## Scaling Options

### Fit to Screen (Default)

```bash
gslapper -o "loop panscan=1.0" DP-1 /path/to/video.mp4
```

### Scale with Factor

```bash
# Scale to 80% of screen size
gslapper -o "loop panscan=0.8" DP-1 /path/to/video.mp4
```

### Stretch to Fill

```bash
gslapper -o "loop stretch" DP-1 /path/to/video.mp4
```

### Original Resolution

```bash
gslapper -o "loop original" DP-1 /path/to/video.mp4
```

## Multiple Monitors

### Same Video on All Monitors

```bash
gslapper -o "loop" '*' /path/to/video.mp4
```

### Different Videos per Monitor

```bash
gslapper -o "loop" DP-1 /path/to/video1.mp4 &
gslapper -o "loop" DP-3 /path/to/video2.mp4 &
```

## Audio

By default, audio is enabled. To disable:

```bash
gslapper -o "loop no-audio" DP-1 /path/to/video.mp4
```

## Background Mode

Run in the background:

```bash
gslapper -f -o "loop" DP-1 /path/to/video.mp4
```

## Supported Formats

- MP4 (H.264, H.265)
- MKV
- WebM
- AVI
- MOV
- Any format supported by GStreamer

!!! note "Codec Support"
    Ensure you have `gst-plugins-ugly` and `gst-libav` installed for H.264/H.265 support.
