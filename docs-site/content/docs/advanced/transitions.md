---
title: Transitions
description: "Animated transitions between static images: fades, wipes, waves and circles."
---

gSlapper can animate the switch from one static image to the next. Pick a type when you start gSlapper or change it later over IPC; every `change` to another image then plays it.

```bash
gslapper --transition-type wipe --transition-duration 1.5 -I /tmp/gslapper.sock DP-1 image1.jpg
echo "change /path/to/image2.jpg" | nc -U /tmp/gslapper.sock
```

## Types

| Type | Effect |
|------|--------|
| `none` | Instant switch (default) |
| `fade` | Crossfade |
| `left`, `right`, `top`, `bottom` | Straight wipe; the new image enters from the named edge |
| `wipe` | Straight wipe travelling at `--transition-angle` |
| `wave` | Wipe with a wavy edge, travelling at `--transition-angle` |
| `grow` | Circle of the new image grows from `--transition-pos` |
| `center` | `grow` from the centre of the screen |
| `outer` | Circle of the old image shrinks towards `--transition-pos` |
| `any` | `grow` from a random position on each change |
| `random` | A random type from `fade` to `any` on each change |

## Options

- `--transition-type TYPE` - one of the types above.
- `--transition-duration SECS` - length of each transition, more than 0 and at most 5 seconds (default 0.5).
- `--transition-angle DEG` - direction the edge travels for `wipe` and `wave`, in degrees clockwise: `0` moves left to right, `90` top to bottom, `180` right to left, `270` bottom to top. Any number is accepted and wrapped into 0-360 (default 45).
- `--transition-pos POS` - circle origin for `grow` and `outer`. Either `x,y` as fractions of the screen from the top-left corner (`0.25,0.75`), or one of `center`, `top`, `bottom`, `left`, `right`, `top-left`, `top-right`, `bottom-left`, `bottom-right` (default `center`).

The same settings are available over IPC: `set-transition`, `set-transition-duration`, `set-transition-angle`, `set-transition-pos` and `get-transition`. See [IPC Control](../user-guide/ipc-control).

## Examples

```bash
# Diagonal wipe from the top-left
gslapper --transition-type wipe --transition-angle 45 -I /tmp/gslapper.sock '*' image.jpg

# Circle growing from the bottom-right corner
gslapper --transition-type grow --transition-pos bottom-right -I /tmp/gslapper.sock DP-1 image.jpg

# Switch to a slow wave at runtime
echo "set-transition wave" | nc -U /tmp/gslapper.sock
echo "set-transition-duration 2" | nc -U /tmp/gslapper.sock
echo "change /path/to/next.jpg" | nc -U /tmp/gslapper.sock
```

### Slideshow script

```bash
#!/bin/bash
SOCKET="/tmp/gslapper.sock"
IMAGES=(/path/to/image1.jpg /path/to/image2.png /path/to/image3.webp)

gslapper --transition-type random --transition-duration 1.5 -I "$SOCKET" DP-1 "${IMAGES[0]}" &
sleep 3

for img in "${IMAGES[@]:1}"; do
    echo "change $img" | nc -U "$SOCKET"
    sleep 5
done
```

## How it works

Transitions run on the GPU. When a change starts, the current image's texture is kept and the new image is decoded into a second texture. A fragment shader draws both, each with its own scaling, so images with different aspect ratios keep their shape and letterbox bars stay black. A mask decides which image shows at each pixel. Masks are computed in screen pixels, so circles stay round on any monitor shape and scale.

The clock starts once the new image is on the GPU, so a large image that takes a while to decode still gets the whole transition. Wipes, waves and circles ease in and out; `fade` is linear.

With several monitors, all of them play the same transition at the same time, each at its own resolution.

## Limits

- Transitions only run between static images. Changes to or from a video or an animated GIF switch instantly.
- A `change` during a running transition finishes it immediately and starts the next one from that image.
- If the new image fails to load, the old image stays on screen and gSlapper keeps running.
- Settings changed over IPC are not saved in the state file. Put them on the command line or in your service file to keep them.
