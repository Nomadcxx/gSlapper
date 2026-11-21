# README demo

`gslapper-demo.webp` is the loop shown under the wordmark in gSlapper's README:

- seven wallpapers linked by six transitions (wave, grow, center, wipe, outer, grow), each 0.6 s on screen with 0.45 s holds between them;
- then three video wallpapers, 2 s each.

Every frame is real gSlapper output captured from a headless compositor.

## Regenerating it

Needs sway, grim, ffmpeg (with `libwebp_anim`), Python 3 and a gSlapper checkout with a build. Keep the frames directory off tmpfs: a run writes about 6 GB of raw frames.

The stills and videos come from `~/Pictures/wallpapers`. They are named in `capture_transitions.py` (`IMAGES`) and below.

```sh
# 1. Transitions: headless sway, each run 4x slower than shown
python3 demo/capture_transitions.py <gslapper-src> <gslapper-src>/build/gslapper ~/Pictures/wallpapers ~/.cache/gslapper-demo/frames

# 2. Videos: cut 3.2 s at 1080p from 0.5 s before the segment, re-timed 8x slower (every frame kept)
ffmpeg -ss 10.0 -t 3.2 -i "IZH80qfT2K5zarO_Space Girl 2 Prob4.mp4" -vf 'scale=1920:1080:flags=lanczos,setpts=8*PTS' -fps_mode passthrough -an -c:v libx264 -crf 12 video-1-slow8.mp4
ffmpeg -ss 9.5 -t 3.2 -i "JnTDdp1g89wIMRy_Synthwave Dreamwave Girl.mp4" -vf 'scale=1920:1080:flags=lanczos,setpts=8*PTS' -fps_mode passthrough -an -c:v libx264 -crf 12 video-2-slow8.mp4
ffmpeg -ss 6.5 -t 3.2 -i Neon_Samurai.mp4 -vf 'scale=1920:1080:flags=lanczos,setpts=8*PTS' -fps_mode passthrough -an -c:v libx264 -crf 12 video-3-slow8.mp4
for i in 1 2 3; do
    python3 demo/capture_video.py <gslapper-src> <gslapper-src>/build/gslapper video-$i-slow8.mp4 ~/.cache/gslapper-demo/frames --name video-$i --slow 8 --length 2.5
done

# 3. Assemble at 60 fps and encode (quality 90, about 47 MB)
python3 demo/build_demo.py ~/.cache/gslapper-demo/frames demo/gslapper-demo.webp
```

## Why the slow-down is faithful

grim can't grab 1080p at 60 fps (about 35 fps with software rendering), so nothing is captured in real time:

- **Transitions:** progress is elapsed time over duration, and the transition shader's only animated input is that progress. A 2.4 s run sped up 4x shows exactly what a 0.6 s run shows.
- **Videos:** gSlapper plays a copy re-timed 8x slower with its frames unchanged. Each rendered frame stays on screen long enough to be grabbed several times. `capture_video.py` keeps the first grab of each, with its time. `build_demo.py` then samples the middle of each 60 fps tick, so 30, 50 and 60 fps clips all play at their real speed.

`python3 -m unittest demo.test_timeline` covers the resampling and frame selection.
