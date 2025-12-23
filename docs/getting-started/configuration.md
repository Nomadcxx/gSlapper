# Configuration

gSlapper uses configuration files compatible with mpvpaper.

## Configuration Directory

Configuration files are located in:

```
~/.config/mpvpaper/
```

## Pause List

Create `~/.config/mpvpaper/pauselist` to specify applications that should pause wallpaper playback when they're in fullscreen:

```
firefox
chromium
mpv
vlc
```

Each line should contain the application name (as it appears in window titles).

## Stop List

Create `~/.config/mpvpaper/stoplist` to specify applications that should stop wallpaper playback:

```
games
```

## Auto-Pause/Stop Options

You can also enable auto-pause/stop via command line:

```bash
# Auto-pause when wallpaper is hidden
gslapper -p -o "loop" DP-1 video.mp4

# Auto-stop when wallpaper is hidden
gslapper -s -o "loop" DP-1 video.mp4
```

## Environment Variables

Currently, gSlapper doesn't support environment variables for configuration. This may be added in future versions.
