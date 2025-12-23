# Scaling Modes

gSlapper provides several scaling modes to control how wallpapers are displayed.

## Fill Mode

**Default for images**

Fills the entire screen while maintaining aspect ratio. Excess content is cropped.

```bash
gslapper -o "fill" DP-1 /path/to/image.jpg
```

**Use case:** Best for wallpapers where you want full screen coverage and don't mind cropping.

## Stretch Mode

Fills the entire screen ignoring aspect ratio. Content may appear distorted.

```bash
gslapper -o "stretch" DP-1 /path/to/image.jpg
```

**Use case:** When you want full coverage and don't care about aspect ratio.

## Original Mode

Displays at native resolution with 1:1 pixel mapping. May show letterboxing/pillarboxing.

```bash
gslapper -o "original" DP-1 /path/to/image.jpg
```

**Use case:** When you want to see the image exactly as it was created, pixel-perfect.

## Panscan Mode

**Default for videos**

Fits content inside the screen with a scaling factor (0.0 to 1.0).

```bash
# Scale to 100% (fit to screen)
gslapper -o "panscan=1.0" DP-1 /path/to/video.mp4

# Scale to 80%
gslapper -o "panscan=0.8" DP-1 /path/to/video.mp4
```

**Use case:** For videos where you want to control the zoom level while maintaining aspect ratio.

## Comparison

| Mode | Aspect Ratio | Coverage | Distortion |
|------|--------------|----------|------------|
| Fill | Maintained | Full | None |
| Stretch | Ignored | Full | Possible |
| Original | Maintained | Partial | None |
| Panscan | Maintained | Variable | None |

## Recommendations

- **Images:** Use `fill` for most cases
- **Videos:** Use `panscan=1.0` (default) for best results
- **Artwork:** Use `original` to preserve pixel-perfect rendering
