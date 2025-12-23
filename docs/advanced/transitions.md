# Transitions

gSlapper supports smooth fade transitions between static images, providing a polished wallpaper switching experience.

## Enabling Transitions

Transitions are enabled via command-line options:

```bash
gslapper --transition-type fade --transition-duration 2.0 -I /tmp/gslapper.sock DP-1 image.jpg
```

### Options

- `--transition-type TYPE` - Transition effect type
  - `none` (default) - No transition
  - `fade` - Smooth fade between images
  
- `--transition-duration SECS` - Transition duration in seconds (default: 0.5)

## Requirements

- **IPC must be enabled** - Transitions require the `-I` or `--ipc-socket` option
- **Static images only** - Transitions work between images, not videos
- **Same or different aspect ratios** - Both images will correctly fill the screen

## Usage

### Basic Fade Transition

```bash
# Start with first image
gslapper --transition-type fade --transition-duration 2.0 -I /tmp/gslapper.sock DP-1 image1.jpg

# Switch to second image (transition happens automatically)
echo "change /path/to/image2.jpg" | nc -U /tmp/gslapper.sock
```

### Different Aspect Ratios

Transitions handle images with different aspect ratios correctly:

```bash
# 4:3 image -> 16:9 image
gslapper --transition-type fade --transition-duration 1.5 -I /tmp/gslapper.sock DP-1 4x3_image.jpg
echo "change /path/to/16x9_image.jpg" | nc -U /tmp/gslapper.sock
```

Both images will fill the screen properly during the transition, with no letterboxing.

## How It Works

1. **Capture** - When a transition starts, the current image is captured
2. **Load** - The new image is loaded asynchronously
3. **Resize** - Both images are resized to display resolution with fill mode
4. **Blend** - Images are blended pixel-by-pixel using CPU-based alpha blending
5. **Complete** - Transition completes when duration is reached

## Supported Formats

Transitions work with all supported image formats:
- JPEG (.jpg, .jpeg)
- PNG (.png)
- WebP (.webp)
- GIF (.gif) - First frame only

## Performance

- **CPU-based blending** - Uses efficient pixel blending similar to swww
- **Display resolution** - Blends at logical display resolution for optimal performance
- **Smooth animation** - Frame rate capped to ensure smooth transitions

## Limitations

- Transitions only work between static images
- Video wallpapers do not support transitions
- IPC socket must be enabled
- Rapid transitions (multiple changes in quick succession) may cancel previous transitions

## Examples

### Slideshow Script

```bash
#!/bin/bash
SOCKET="/tmp/gslapper.sock"
IMAGES=(
    "/path/to/image1.jpg"
    "/path/to/image2.png"
    "/path/to/image3.webp"
)

# Start with first image
gslapper --transition-type fade --transition-duration 2.0 -I "$SOCKET" DP-1 "${IMAGES[0]}" &
sleep 3

# Cycle through images
for img in "${IMAGES[@]:1}"; do
    echo "change $img" | nc -U "$SOCKET"
    sleep 3
done
```

### Random Wallpaper with Transition

```bash
#!/bin/bash
SOCKET="/tmp/gslapper.sock"
WALLPAPER_DIR="/home/user/Pictures/wallpapers"

# Get random image
RANDOM_IMG=$(find "$WALLPAPER_DIR" -type f \( -iname "*.jpg" -o -iname "*.png" -o -iname "*.webp" \) | shuf -n 1)

# Start or change wallpaper
if pgrep -x gslapper > /dev/null; then
    echo "change $RANDOM_IMG" | nc -U "$SOCKET"
else
    gslapper --transition-type fade --transition-duration 1.5 -I "$SOCKET" DP-1 "$RANDOM_IMG"
fi
```
