# swww vs gSlapper Systemd Service Comparison

## swww Architecture

**swww** uses a **daemon-client** model:

1. **`swww-daemon`** - Long-running daemon process
   - Runs continuously, listening on IPC socket
   - Manages wallpapers for all outputs
   - Systemd service just starts: `swww-daemon`
   - No `--restore` flag needed - daemon handles state internally

2. **`swww` (client)** - Command-line tool
   - Sends commands to daemon via IPC socket
   - Examples: `swww img /path/to/image.jpg`, `swww query`

**Systemd Service:**
```ini
[Service]
Type=simple
ExecStart=/usr/bin/swww-daemon
# That's it! The daemon runs forever
```

## gSlapper Current Architecture

**gSlapper** uses a **one-process-per-wallpaper** model (like mpvpaper):

1. **`gslapper`** - Single process per wallpaper
   - Each instance manages one output
   - Optional IPC per instance
   - Process exits when wallpaper stops

2. **Multi-monitor** - Requires multiple instances
   - One `gslapper` process per monitor
   - Each has its own IPC socket (optional)

**Current Systemd Approach (WRONG):**
- Wrapper script to launch multiple instances ❌
- Complex state file parsing in shell ❌

## What We Should Do

### Option 1: Match swww Exactly (Daemon Mode)
**Requires significant refactoring:**
- Create `gslapper-daemon` that manages all outputs
- Single long-running process
- IPC for all commands
- Internal state management

**Pros:** Matches swww exactly  
**Cons:** Major architectural change, breaks existing usage

### Option 2: Simple Service Per Monitor (Current + Fix)
**Keep current architecture, fix service:**
- One service instance per monitor (like systemd templates)
- Service file: `gslapper@.service` (template)
- Each instance: `gslapper --systemd --restore DP-1 /path/to/wallpaper`
- State restoration built into gslapper

**Pros:** Minimal changes, works with current architecture  
**Cons:** Multiple services for multiple monitors

### Option 3: Single Service with '*' Output (Simplest)
**Use gslapper's existing '*' feature:**
- One service that restores to all monitors
- Service: `gslapper --systemd --restore '*' /path/to/wallpaper`
- State file: `state.txt` (default, no output name)
- Works if user wants same wallpaper on all monitors

**Pros:** Simplest, no wrapper needed  
**Cons:** Only works for same wallpaper on all monitors

## Recommendation: Option 2 + Documentation

1. **Remove wrapper script** - Not needed, not how swww works
2. **Provide example service file** - Show users how to set it up
3. **Document multi-monitor setup** - Explain systemd template services
4. **Keep state restoration** - Built into gslapper, works automatically

**Example Service File (per monitor):**
```ini
[Unit]
Description=gSlapper Wallpaper Service for %i
After=graphical-session.target

[Service]
Type=notify
ExecStart=/usr/bin/gslapper --systemd --restore %i
EnvironmentFile=%h/.config/gslapper/environment
Environment=XDG_RUNTIME_DIR=%t

[Install]
WantedBy=default.target
```

**Usage:**
```bash
# Enable for DP-1
systemctl --user enable gslapper@DP-1.service

# Enable for DP-3  
systemctl --user enable gslapper@DP-3.service
```

This matches swww's simplicity - just a service file, no wrapper scripts.
