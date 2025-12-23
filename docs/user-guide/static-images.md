# Static Images

gSlapper supports static images with automatic format detection and multiple scaling modes.

## Supported Formats

- **JPEG** (.jpg, .jpeg)
- **PNG** (.png)
- **WebP** (.webp)
- **GIF** (.gif) - First frame only

## Basic Usage

```bash
gslapper DP-1 /path/to/wallpaper.jpg
```

Images default to **fill mode**, which fills the screen while maintaining aspect ratio (cropping excess).

## Scaling Modes

### Fill Mode (Default)

Fills the screen maintaining aspect ratio, crops excess:

```bash
gslapper -o "fill" DP-1 /path/to/image.png
```

### Stretch Mode

Fills screen ignoring aspect ratio:

```bash
gslapper -o "stretch" DP-1 /path/to/image.png
```

### Original Resolution

Display at native resolution (1:1 pixel mapping):

```bash
gslapper -o "original" DP-1 /path/to/image.png
```

### Panscan Mode

Fit inside screen with scaling factor:

```bash
gslapper -o "panscan=0.8" DP-1 /path/to/image.png
```

## Multiple Monitors

```bash
# Same image on all monitors
gslapper -o "fill" '*' /path/to/wallpaper.jpg

# Different images per monitor
gslapper -o "fill" DP-1 /path/to/image1.jpg &
gslapper -o "fill" DP-3 /path/to/image2.jpg &
```

## Transitions

gSlapper supports smooth fade transitions between images. See [Transitions](../advanced/transitions.md) for details.
