# Systemd Service Analysis: Why Do We Need It?

## What Does the Systemd Service Actually Do?

### 1. Automatic Startup on Login
```ini
After=graphical-session.target
Wants=graphical-session.target
```
- Starts gSlapper automatically when graphical session is ready
- No need to manually run `gslapper` after each login/reboot

### 2. Environment Variable Management
```ini
EnvironmentFile=%h/.config/gslapper/environment
Environment=XDG_RUNTIME_DIR=%t
```
- Provides `WAYLAND_DISPLAY` and `XDG_RUNTIME_DIR` automatically
- Ensures Wayland connection works in systemd context
- Without this, gSlapper can't connect to Wayland compositor

### 3. Automatic Restart on Failure
```ini
Restart=on-failure
RestartSec=5
```
- If gSlapper crashes or exits unexpectedly, systemd restarts it
- Provides resilience - wallpaper keeps running

### 4. State Restoration
```ini
ExecStart=/usr/bin/gslapper --systemd --restore '*'
```
- `--restore` flag loads saved state from `~/.local/state/gslapper/`
- Resumes wallpaper at last position (videos) or shows last image
- Without this, you'd need to manually set wallpaper after each boot

### 5. Process Lifecycle Management
- `systemctl start/stop/restart` - Control wallpaper service
- `systemctl reload` - Saves state and restarts (SIGHUP)
- `journalctl` - View logs
- Integration with system shutdown/reboot

### 6. Systemd Notifications (--systemd flag)
```c
#ifdef HAVE_SYSTEMD
    if (systemd_mode) {
        sd_notify(0, "READY=1");
    }
#endif
```
- Tells systemd when gSlapper is ready
- Allows `Type=notify` for better startup coordination
- Systemd knows when wallpaper is actually displayed (not just process started)

## Why swww Doesn't Need Systemd Service

**swww-daemon architecture:**
- **Long-running daemon** - Process stays alive indefinitely
- **Manual startup** - Users run `swww-daemon &` in shell config or manually
- **Persistent** - Once started, it runs until killed
- **No state restoration needed** - Daemon maintains state in memory

**Why swww users might still use systemd:**
- Convenience - automatic startup
- Process management - restart on failure
- But it's **optional** - daemon works fine without it

## Why gSlapper Benefits from Systemd Service

**gSlapper architecture:**
- **One process per wallpaper** - Process exits when wallpaper stops
- **Not a daemon** - Designed to exit when done (like mpvpaper)
- **State-based** - Relies on saved state files for restoration
- **Needs restart** - Must be restarted after reboot to restore wallpaper

**Without systemd service:**
```bash
# User must manually do this after every login:
gslapper --restore DP-1
# or
gslapper -o "loop" DP-1 /path/to/video.mp4
```

**With systemd service:**
- Automatic - wallpaper restores on login
- No manual intervention needed
- Works seamlessly with state restoration

## Can We Just Run `gslapper --restore` Without Systemd?

**Yes, technically:**
```bash
# In ~/.bashrc or ~/.zshrc:
gslapper --restore DP-1 &
```

**But you lose:**
1. ❌ Automatic restart on failure
2. ❌ Proper environment variable management
3. ❌ Process lifecycle management
4. ❌ Integration with system shutdown
5. ❌ Logging via journalctl
6. ❌ Coordination with graphical session startup
7. ❌ Resource limits and security settings

**Advantages of shell script approach:**
1. ✅ Simpler - no systemd knowledge needed
2. ✅ Works on systems without systemd
3. ✅ More control over startup timing

**Advantages of systemd service:**
1. ✅ Automatic startup on login
2. ✅ Automatic restart on failure
3. ✅ Proper environment management
4. ✅ Process lifecycle management
5. ✅ Better integration with system
6. ✅ Logging and monitoring
7. ✅ Resource limits and security

## Comparison: With vs Without Systemd

### Without Systemd (Manual/Shell Script)
```bash
# User must:
1. Create environment file manually
2. Run gslapper in shell config or manually
3. Handle failures manually
4. No automatic restart
5. No process management
```

### With Systemd Service
```bash
# User:
1. Enable service once: systemctl --user enable gslapper.service
2. Done - automatic on every login
3. Automatic restart on failure
4. Full process management
5. Integrated with system
```

## What --systemd Flag Actually Does

The `--systemd` flag enables systemd-specific features:

1. **Systemd notifications** - `sd_notify(READY=1)` when wallpaper is ready
2. **Proper exit handling** - Notifies systemd when stopping
3. **Signal handling** - SIGHUP for reload (saves state, restarts)
4. **Logging context** - Better integration with journalctl

**Without --systemd flag:**
- gSlapper still works
- But systemd doesn't know when it's "ready"
- `Type=notify` won't work properly
- Less integration with systemd lifecycle

**With --systemd flag:**
- Full systemd integration
- Proper readiness notification
- Better process management

## Recommendation

**We SHOULD provide a default systemd service because:**

1. **gSlapper is not a daemon** - Unlike swww-daemon, it exits when done
2. **State restoration requires restart** - Need to restart after reboot
3. **Better user experience** - Automatic wallpaper on login
4. **Process management** - Restart on failure, proper lifecycle
5. **Environment management** - Wayland variables handled correctly
6. **Standard practice** - Most Wayland wallpaper tools use systemd

**But we should:**
- Make it easy to customize (template service, clear docs)
- Provide instructions for manual setup (shell script alternative)
- Explain advantages/disadvantages clearly
- Allow users to disable if they prefer manual control

## Implementation Strategy

1. **Default service** - Install and enable by default (post-install hook)
2. **Template service** - Provide `gslapper@.service` for per-monitor
3. **Clear documentation** - Explain what it does and how to customize
4. **Comments in service file** - Inline instructions for customization
5. **Alternative docs** - Show shell script approach for non-systemd users
