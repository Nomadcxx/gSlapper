# README demo

`gslapper-demo.webp` is the loop shown under the wordmark in gSlapper's README: wave, grow, center and wipe transitions between three wallpapers, then four seconds of video. Every frame is real gSlapper output captured from a headless compositor.

## Regenerating it

Needs sway, grim, ffmpeg (with `libwebp_anim`), Python 3 and a gSlapper checkout with a build. Keep the frames directory off tmpfs: a run writes about 6 GB of raw frames.

```sh
# 1. Transitions: headless sway, each transition run 4x slower than shown
python3 demo/capture_transitions.py <gslapper-src> <gslapper-src>/build/gslapper <media-dir> ~/.cache/gslapper-demo/frames

# 2. Video: cut the segment, re-time it 4x slower, capture on a GPU-rendered sway
ffmpeg -stream_loop 1 -ss 18.0 -t 6 -i jackal-class-troublemaker-nikke-moewalls-com.mp4 -an -c:v libx264 -crf 12 nikke-cut.mp4
ffmpeg -i nikke-cut.mp4 -vf 'setpts=4*PTS' -r 15 -an -c:v libx264 -crf 12 nikke-cut-slow4.mp4
python3 demo/capture_video.py <gslapper-src> <gslapper-src>/build/gslapper nikke-cut-slow4.mp4 ~/.cache/gslapper-demo/frames --slow 4 --gpu-device /dev/dri/renderD128

# 3. Assemble at 60 fps and encode (quality 95, about 30 MB)
python3 demo/build_demo.py ~/.cache/gslapper-demo/frames demo/gslapper-demo.webp
```

`<media-dir>` holds `19 - BaDWxwe.jpg` (opening image), `wp14824417-1920x1080-wallpapers.webp` and `148 - DJPJkF0.jpg`. The 2026-11 video capture ran on an Intel UHD 620 laptop; the desktop's NVIDIA card can't drive a headless GPU sway.

## Why the slow-down is faithful

grim can't grab 1080p at 60 fps (about 35 fps with software rendering, about 30 fps on a GPU), so neither part is captured in real time:

- Transition progress is elapsed time over duration, and the transition shader's only animated input is that progress. A 4.8 s run sped up 4x shows exactly what a 1.2 s run shows.
- The video is fed to gSlapper re-timed 4x slower with its frames unchanged. Each rendered frame is grabbed at least once, and `build_demo.py` keeps the first grab of each, so the clip plays back at its real 60 fps.

`python3 -m unittest demo.test_timeline` covers the resampling and frame selection.
