# Handover: Restore, IPC, and Consumer Compatibility

Date: 2026-07-08
Branch: `fix/restore-ipc-state-compat`
PR: https://github.com/Nomadcxx/gSlapper/pull/17
Implementation commit: `85d232d`

## Scope

This session focused on pre-release correctness around restore/state behavior and compatibility with projects that invoke gSlapper as a backend. The work intentionally stayed narrow: command-line restore handling, IPC command behavior, state file durability, documentation alignment, and consumer smoke checks.

## Changes Made

### Restore CLI

- `gslapper --restore` now accepts the documented restore shape without requiring a normal wallpaper positional argument.
- `gslapper --restore [output]` is now shown in `--help`.
- Wildcard/all-output restore selectors (`*`, `all`, `All`, `ALL`) restore from the default `state.txt` instead of an output-specific `state-_.txt`.
- `--state-file PATH` is now honored during restore.
- If restore fails and no fallback wallpaper was supplied, gSlapper exits with a clear error instead of falling through ambiguously.
- Restore with a fallback wallpaper remains supported: `gslapper --restore <output> <fallback>`.

### State Saving

- State saving for wildcard/all-output runs now records the logical selector while still using the default state file path.
- The atomic writer in `src/state.c` now uses a unique temporary file (`mkstemp`) instead of a fixed `<state>.tmp` path.
- This fixes a real local failure where an abandoned `state.txt.tmp` permanently blocked all future saves with `Failed to create temp state file`.
- Temp files are still created in the same directory and renamed into place, preserving atomic replacement semantics.
- Temp state file permissions are explicitly set to `0600`.

### IPC

- Added IPC `quit` as an alias for `stop`.
- Added the documented IPC `save-state` command.
- Updated IPC help text to list `stop, quit` and `save-state`.
- Fixed IPC response fd ownership:
  - The client thread now duplicates the accepted client fd for each queued command.
  - The main loop owns that duplicate until it sends the response.
  - The queued command cleanup closes the response fd.
- This fixes the observed behavior where `socat` received an empty response and gSlapper logged `Failed to send IPC response: Bad file descriptor`.
- Multiple newline-delimited commands on one socket connection were tested and still work.

### Documentation

- Updated `docs/user-guide/ipc-control.md`:
  - Documents `stop` / `quit`.
  - Documents IPC `save-state`.
- Updated `docs/development/api-reference.md`:
  - Adds `quit` beside `stop`.
  - Adds `save-state`.

## Consumer Compatibility Assessment

### Waytrogen

Repository checked: https://github.com/nikolaizombie1/waytrogen

Current Waytrogen gSlapper integration invokes gSlapper approximately as:

```bash
gslapper -I /tmp/gslapper.sock [-p|-s] -o "<scale loop no-audio additional>" -f <monitor|*> <image>
```

It also tries to stop an existing gSlapper instance with:

```bash
echo quit | socat - UNIX-CONNECT:/tmp/gslapper.sock
```

and falls back to `pkill -9 gslapper`.

Assessment:

- The launch shape remains compatible.
- The new IPC `quit` alias directly supports Waytrogen's graceful shutdown path.
- Before this fix, Waytrogen would rely on its forced-kill fallback because gSlapper did not handle `quit`.
- After this fix, `quit` returns `OK` and exits cleanly in local live IPC testing.

### Waypaper

Repository checked: https://github.com/anufrievroman/waypaper

Current Waypaper gSlapper integration invokes gSlapper approximately as:

```bash
gslapper --fork -o "loop <fill-mode> [no-audio] [user-options]" <monitor|*> <path>
```

It does not use gSlapper IPC for normal operation. It stops gSlapper with process killing (`killall gslapper` for all outputs or monitor-pattern matching for a specific output).

Assessment:

- The launch shape remains compatible.
- Local smoke tests started Waypaper-shaped commands with these option variants: `panscan=1.0`, `original`, `stretch`, and `fill`.
- No parser, startup, or immediate runtime errors were observed for those command shapes.
- There is one semantic mismatch worth noting: Waypaper maps its UI `fill` option to gSlapper `panscan=1.0`. In current gSlapper semantics, `panscan=1.0` behaves like contain/fit, while gSlapper `fill` is cover/crop. This is not a gSlapper release blocker, but it is a good future upstream fix for Waypaper's backend mapping.

## Local Verification

Ran on this laptop against the rebuilt local binary:

```bash
ninja -C build
tests/test_basic.sh
```

Result:

- Build passed.
- Basic integration tests passed: 16 passed, 0 failed.
- Existing warning remains in `output_description`: discards `const` qualifier. This was left out of scope.

Live checks performed:

- `gslapper --restore --state-file /tmp/gslapper-definitely-missing`
  - Exits `1` with explicit missing-state/fallback error.
- `gslapper --restore '*'`
  - No longer fails with the old positional-argument error.
- IPC over Unix socket:
  - `query` returns status.
  - `save-state` returns `OK: state saved`.
  - `help` lists the expected commands.
  - `quit` returns `OK` and exits.
  - Multi-command socket input returned responses correctly.
- Waypaper-shaped launch commands:
  - `loop panscan=1.0 no-audio`
  - `loop original no-audio`
  - `loop stretch no-audio`
  - `loop fill no-audio`

The rebuilt binary was installed to the active user path:

```bash
/home/nomadx/.local/bin/gslapper
```

The installed user-local binary hash matched `build/gslapper` after installation.

## Process Hygiene

Extra smoke-test gSlapper processes were cleaned up. The only gSlapper process intentionally left running was the existing sysc-greet wallpaper instance:

```bash
gslapper -f -I /tmp/sysc-greet-wallpaper.sock * /usr/share/sysc-greet/wallpapers/sysc-greet-dark.png
```

## Follow-Ups

- Review PR #17 on a second environment before merging.
- Consider an upstream Waypaper issue or PR to map Waypaper UI `fill` to gSlapper `fill` instead of `panscan=1.0`.
- Consider a small follow-up cleanup for old backup files under `src/`, because they pollute search results with stale `clappie` and old IPC/state behavior.
- Consider separately fixing the existing `output_description` `const` warning.
