---
title: Contributing
description: "Thank you for your interest in contributing to gSlapper! This guide will help you get started."
---

Thank you for your interest in contributing to gSlapper! This guide will help you get started.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork**:
   ```bash
   git clone https://github.com/YOUR_USERNAME/gSlapper.git
   cd gSlapper
   ```
3. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Setup

See [Building from Source](building) for setup instructions.

## Code Style

- **C code** follows K&R style with 4-space indentation
- **GStreamer objects** use `g_` prefix (GLib naming)
- **Wayland objects** use `wl_` prefix
- **Static functions** at file scope
- **Global state** in `global_state` pointer
- **Thread communication** via `wakeup_pipe`

## Making Changes

### Adding New Features

1. **Plan your changes** - Consider architecture and impact
2. **Update documentation** - Add/update relevant docs
3. **Add tests** - Create test scripts if applicable
4. **Test thoroughly** - Test on different compositors and setups

### Adding New IPC Commands

1. **Define command handler** in `src/main.c`'s `process_ipc_commands()` function
2. **Parse command arguments** from `cmd_line` string
3. **Validate inputs** and check state
4. **Implement command logic** with thread safety in mind
5. **Send response** via `ipc_send_response(client_fd, response)`
6. **Free command** with `free(cmd->cmd_line)` and `free(cmd)`

See existing commands like `set-transition` or `change` for examples.

### Adding New Transition Effects

1. **Add transition type** to `transition_type_t` enum
2. **Implement transition function** (similar to `update_transition()`)
3. **Update command parser** in IPC `set-transition` handler
4. **Add command-line option** parsing in `parse_command_line()`
5. **Test** with automated and visual tests

### Modifying Rendering Pipeline

1. Locate rendering code in `render()` function
2. Ensure EGL context is current before OpenGL calls
3. Lock `video_mutex` when accessing `video_frame_data` or `transition_state`
4. Update vertex data with `update_vertex_data()` for scaling changes

## Testing

### Basic Functionality

```bash
# Test video playback
./build/gslapper -v DP-1 /path/to/video.mp4

# Test image display
./build/gslapper -v DP-1 /path/to/image.jpg

# Test with IPC enabled
./build/gslapper -I /tmp/test.sock -o "loop" DP-1 video.mp4
```

### Test Scripts

Run the included test scripts:

```bash
# Basic integration tests
./tests/test_basic.sh

# Memory safety tests (requires ASAN build or valgrind)
./tests/test_memory.sh

# Systemd service tests
./tests/test_systemd.sh
```

### IPC Testing

```bash
# Start with IPC enabled
./build/gslapper -I /tmp/test.sock -vv -o "loop" DP-1 video.mp4

# From another terminal, test commands
echo "query" | nc -U /tmp/test.sock
echo "pause" | nc -U /tmp/test.sock
echo "resume" | nc -U /tmp/test.sock
```

## Commit Guidelines

- **Write clear commit messages** - Explain what and why
- **Keep commits focused** - One logical change per commit
- **Test before committing** - Ensure changes work
- **Update documentation** - Keep docs in sync with code

### Commit Message Format

```
Short summary (50 chars or less)

More detailed explanation if needed. Wrap at 72 characters.
Explain what the change does and why.

- Bullet points for multiple changes
- Reference issues if applicable
```

## Pull Request Process

1. **Ensure your code works** - Test on your system
2. **Update documentation** - Keep docs current
3. **Write a clear PR description** - Explain changes and motivation
4. **Reference issues** - Link to related issues if applicable
5. **Wait for review** - Be responsive to feedback

## Areas for Contribution

- **Bug fixes** - Report and fix issues
- **Performance improvements** - Optimize rendering or memory usage
- **New features** - Add requested functionality
- **Documentation** - Improve docs and examples
- **Testing** - Add test coverage
- **Code quality** - Refactor and improve code

## Reporting Issues

When reporting issues, please include:

- **gSlapper version** - `gslapper --version` or git commit
- **System information** - OS, compositor, GPU
- **Steps to reproduce** - Clear reproduction steps
- **Expected behavior** - What should happen
- **Actual behavior** - What actually happens
- **Logs** - Output with `-vv` flag if applicable

## Release Process

The in-repo `PKGBUILD` and `gslapper.install` are the canonical templates for the AUR package; the AUR copies must stay identical to them.

For every release `vX.Y.Z`, in order:

1. Bump the version in all three files: `meson.build`, `PKGBUILD` (`pkgver`), and `flake.nix`.
2. Update `gslapper.install`:
   - Rewrite the "New in X.Y.Z" notes in `post_upgrade` for this release.
   - Keep the brand ASCII banner in **both** `post_install` and `post_upgrade`. It was lost from `post_upgrade` in the 1.4.0 update; do not lose it again.
3. Commit to master, push, then create and push the annotated tag `vX.Y.Z`. The tag triggers the release workflow: per-distro builds (Ubuntu, Debian, Fedora), nfpm deb/rpm packaging, in-container install verification, and a GitHub Release with SHA256SUMS. Confirm every job succeeds before continuing.
4. After the tag exists, download `https://github.com/Nomadcxx/gSlapper/archive/vX.Y.Z.tar.gz`, put its sha256 into `PKGBUILD` `sha256sums`, and commit that to master. The tag's own tarball necessarily carries the previous hash; master is what the AUR template is copied from, so master must be correct.
5. If linked libraries changed, update `depends` in `PKGBUILD` and the dependency lists in the release workflow (check with `ldd build/gslapper`; example: `systemd-libs` was added in 1.5.1 for libsystemd).
6. Update the AUR package (`ssh://aur@aur.archlinux.org/gslapper.git`):
   - Copy `PKGBUILD` and `gslapper.install` from the repo.
   - Verify with a real build: `makepkg -f` must download the tag tarball, pass the checksum, and build.
   - Regenerate metadata: `makepkg --printsrcinfo > .SRCINFO`.
   - Commit and push. Never commit tarballs, `pkg/`, or `src/`.
7. Verify the release is live: the AUR cgit `.SRCINFO` shows the new `pkgver` (the RPC API caches for a few minutes), and the GitHub Release lists the deb, rpm, and SHA256SUMS assets.

## Questions?

Feel free to open an issue for questions or discussions about contributions.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
