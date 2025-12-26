# How swww Handles Wallpaper Restoration After Reboot

## Key Finding: swww Uses a Cache System

Based on the swww source code and documentation, here's how swww handles restoration:

### 1. **Built-in Cache System**

swww-daemon has a **cache system** that stores the last displayed image for each output:

- **Cache location**: Likely in `~/.cache/swww/` or similar
- **Automatic restoration**: When daemon starts, it automatically loads cached images
- **Per-output cache**: Each monitor has its own cached wallpaper

**From swww-daemon.1.scd:**
```
*--no-cache*
	Don't search the cache for the last wallpaper for each output.
	Useful if you always want to select which image 'swww' loads manually using
	'swww img'
```

This means:
- **By default**: swww-daemon automatically restores cached wallpapers on startup
- **With --no-cache**: Daemon starts without restoring (blank/black screen)

### 2. **swww-restore Command**

swww provides a `swww restore` command that explicitly restores cached wallpapers:

**From swww-restore.1.scd:**
```
Restores the last displayed image on the specified outputs.

This can be used to split initialization (with *swww init --no-daemon*) and
cache loading into different steps, in order to avoid race condition traps.

You can also use this command to restore the last displayed image when
reconnecting a monitor.
```

**Usage:**
```bash
swww restore                    # Restore all outputs
swww restore -o DP-1,DP-3      # Restore specific outputs
swww restore -n namespace       # Restore specific namespace
```

### 3. **Common Restoration Methods**

#### Method 1: Automatic (Default Behavior)
```bash
# Just start the daemon - it automatically restores cached wallpapers
swww-daemon
```

**How it works:**
- Daemon reads cache files on startup
- Automatically displays last wallpaper for each output
- No manual intervention needed

#### Method 2: Systemd Service (Most Common)
```ini
[Service]
ExecStart=/usr/bin/swww-daemon
```

**What happens:**
1. Systemd starts `swww-daemon` on login
2. Daemon automatically reads cache and restores wallpapers
3. Wallpapers appear automatically - no `swww restore` needed

#### Method 3: Manual Restore Command
```bash
# Start daemon without cache
swww-daemon --no-cache

# Later, manually restore
swww restore
```

**Use case:** When you want to control when restoration happens

#### Method 4: Shell Script in Startup
```bash
# In ~/.bashrc or ~/.zshrc
swww-daemon &
swww restore  # Explicit restore (optional - daemon does it automatically)
```

### 4. **Cache vs State Files**

**swww approach:**
- **Cache files** - Store last displayed image path per output
- **In-memory state** - Daemon keeps state while running
- **No explicit state files** - Just cache for restoration

**gSlapper approach (current):**
- **State files** - Store wallpaper path, options, position, pause state
- **Explicit restoration** - `--restore` flag loads state
- **More detailed** - Includes video position, options, etc.

### 5. **Why swww Doesn't Need Systemd Service**

**swww-daemon:**
- **Long-running process** - Stays alive indefinitely
- **Automatic cache restoration** - Restores on startup by default
- **Manual startup works** - Users can just run `swww-daemon &` in shell config
- **Systemd is optional** - Convenience, not requirement

**Common swww setup patterns:**

**Pattern A: Systemd Service (Recommended)**
```ini
[Service]
ExecStart=/usr/bin/swww-daemon
```
- Automatic startup on login
- Automatic cache restoration
- Process management

**Pattern B: Shell Script**
```bash
# ~/.bashrc or ~/.zshrc
swww-daemon &
```
- Manual startup
- Still gets automatic cache restoration
- Simpler, no systemd knowledge needed

**Pattern C: Explicit Restore**
```bash
swww-daemon --no-cache &
swww restore  # Explicit restore when ready
```
- Control over when restoration happens
- Useful for race condition avoidance

### 6. **Key Differences: swww vs gSlapper**

| Feature | swww | gSlapper |
|---------|------|----------|
| **Architecture** | Daemon (long-running) | One-shot process |
| **Restoration** | Automatic cache on startup | Explicit `--restore` flag |
| **State Storage** | Cache files (image paths) | State files (path, options, position) |
| **After Reboot** | Daemon auto-restores cache | Must restart with `--restore` |
| **Systemd Need** | Optional (convenience) | **Required** (for auto-restore) |
| **Manual Startup** | Works fine (`swww-daemon &`) | Works but no auto-restore |

### 7. **Why gSlapper Needs Systemd More Than swww**

**swww:**
- Daemon stays alive - can be started manually
- Automatic cache restoration - works without systemd
- Systemd is just for convenience (auto-start, process management)

**gSlapper:**
- Process exits when done - needs restart after reboot
- No automatic restoration - requires `--restore` flag
- **Systemd is essential** - Without it, users must manually run `gslapper --restore` after every login

### 8. **Recommendation for gSlapper**

**We should:**
1. ✅ **Keep systemd service** - Essential for automatic restoration
2. ✅ **Keep state files** - More detailed than swww's cache (video position, etc.)
3. ✅ **Keep `--restore` flag** - Explicit restoration (like `swww restore`)
4. ✅ **Document manual alternative** - Show shell script approach for non-systemd users

**We could add (future enhancement):**
- Automatic restoration on startup (like swww's cache)
- But this conflicts with our architecture (one-shot process)

**Current approach is correct:**
- Systemd service provides automatic restoration
- Matches user expectations (wallpaper on login)
- More powerful than swww (video position, options, etc.)
