# gSlapper: Frame Path Performance + Distro Packaging — Design

Date: 2026-07-08
Status: Approved

## Goal

Three independent deliverables, each its own PR against `master`:

1. **PR 1 — Frame path optimization**: eliminate per-frame heap allocation, frame
   copies, and caps re-parsing in the GStreamer → OpenGL hand-off.
2. **PR 2 — Async texture upload**: double-buffered PBO uploads so texture DMA
   overlaps rendering instead of blocking the render thread.
3. **PR 3 — Release packaging CI**: GitHub Actions workflow that auto-builds
   Debian/Ubuntu `.deb` and Fedora `.rpm` packages on tagged releases, so distros
   beyond Arch/Nix get native packages (and sysc-greet's installer can stop
   building gslapper from source).

## Background

The per-frame hot path today (`src/main.c`):

- `buffer_probe()` (line ~1693) runs on every decoded frame and, while holding
  `video_mutex`:
  - calls `gst_pad_get_current_caps()` and string-parses the format — per frame;
  - `g_free()`s the previous frame and `g_memdup2()`s the entire RGBA frame
    (~14 MB @ 1440p, ~33 MB @ 4K, up to 60×/sec).
- `render()` (line ~973) uploads via `glTexSubImage2D` from client memory, which
  blocks while the driver copies, then `g_free()`s the frame.
- The pipeline uses CPU `videoconvert` to RGBA. A full GL/dmabuf zero-copy path
  was considered and deliberately deferred: it reworks the entire render path
  and needs extended NVIDIA+Wayland soak testing. The two PRs below are the
  safe, benchmarkable pair.

There is no CI in the repo. sysc-greet's `release.yml` (cagebreak matrix job:
per-distro containers + nfpm) is the proven template for packaging a C project
with distro-versioned native deps.

## PR 1 — Frame path: eliminate per-frame allocation and caps churn

**Branch:** `perf/frame-path`

Changes in `src/main.c`, applied to all three pipeline variants (playbin,
filesrc/decodebin video, image pipeline):

1. **Caps caching.** Store the last-seen `GstCaps *` per probe. Re-parse
   format/width/height only when the caps pointer differs from the cached one
   (pointer compare per frame; parse only on renegotiation).
2. **Retained buffer refs instead of copies.** In the probe: `gst_buffer_ref()`
   the incoming buffer, swap it into the shared frame slot (unreffing any
   not-yet-consumed previous buffer). In the render path: map the buffer
   (`GST_MAP_READ`), upload, unmap, unref. Zero CPU-side frame copies.
   - Safety: appsink runs `drop=TRUE, max-buffers=1`; holding one ref is safe
     against pool starvation. If a pool stall surfaces in testing, fallback is
     a persistent reused allocation (realloc only on size change) — still
     removes the malloc/free churn.
3. **Minimal mutex hold.** The mutex-held region shrinks to the
   pointer/metadata swap; no allocation, no caps query, no copy under lock.
4. **Frame cache interaction.** `src/cache.c` consumers that duplicate frame
   data (`g_memdup2(cached->...)`) keep their copies — caching is cold path.
   Only the per-frame hot path changes.

**Verification:** `benchmark.sh` before/after (expect lower avg CPU, flat RAM);
visual check on both monitors; all scaling modes (default/stretch/original);
seamless looping; pause/resume; IPC layer switching.

## PR 2 — Async texture upload via double-buffered PBOs

**Branch:** `perf/pbo-upload`

Changes in `src/main.c`, integrated with the existing smart texture manager:

1. Two `GL_PIXEL_UNPACK_BUFFER` objects sized to the frame, reallocated only on
   dimension change (same policy as the texture).
2. Ping-pong per frame: orphan (`glBufferData(..., NULL, GL_STREAM_DRAW)`), map
   (`glMapBufferRange` with `GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT`),
   write frame N, unmap, `glTexSubImage2D(..., offset 0)` from the bound PBO —
   the driver DMAs asynchronously while frame N−1 renders.
3. **Fallback:** if PBO allocation/map fails, log once and use the direct
   `glTexSubImage2D` path permanently (protects the core-profile fallback
   context and older drivers).
4. Cleanup of PBOs in `cleanup_texture_manager()` / `exit_cleanup()`.

**Ordering:** builds on PR 1 (uploads read from mapped GstBuffer instead of the
old copied allocation). PR 2 branches from PR 1.

**Verification:** benchmark (GPU utilization, frame pacing at 60 fps cap);
transition renderer regression test (shares the texture path); `-vv` frame
metrics comparison.

## PR 3 — Release packaging workflow (deb/rpm)

**Branch:** `ci/release-packages`

New files:

1. **`.github/workflows/release.yml`** — triggered on `v*` tags plus
   `workflow_dispatch` (dry run: builds everything, skips release creation).
   - **Matrix job** (containerized builds):
     | Image | Packager | Artifact |
     |---|---|---|
     | `ubuntu:24.04` | deb | `gslapper_${VER}_ubuntu24.04_amd64.deb` |
     | `debian:13` | deb (serves Ubuntu 25.x too) | `gslapper_${VER}_debian13_amd64.deb` |
     | `fedora:42` | rpm | `gslapper-${VER}-1.fedora42.x86_64.rpm` |
   - Steps per leg: install build deps (meson, ninja, wayland-protocols,
     GStreamer dev, EGL dev), `meson setup build --prefix=/usr`,
     `ninja -C build`, smoke test (`./build/gslapper --help` exit code under no
     Wayland display), install nfpm pinned release, append per-distro runtime
     `depends:` list items to `nfpm.yaml` (same append trick as
     `nfpm-cagebreak.yaml`), package, upload artifact.
   - **Release job** (tags only): download artifacts, `sha256sum` →
     `SHA256SUMS`, publish with `softprops/action-gh-release` and generated
     release notes.
2. **`nfpm.yaml`** — version from `${VERSION}` env; contents:
   - `build/gslapper` → `/usr/bin/gslapper` (0755)
   - `build/gslapper-holder` → `/usr/bin/gslapper-holder` (0755)
   - `gslapper.service`, `gslapper@.service` → `/usr/lib/systemd/user/`
   - `LICENSE` → `/usr/share/licenses/gslapper/LICENSE`
   - ends with a bare `depends:` key for CI to append per-distro entries.
3. Per-distro runtime depends (verified in-container during implementation via
   `ldd` on the built binary, not assumed):
   - deb (approx): `libgstreamer1.0-0`, `libgstreamer-plugins-base1.0-0`,
     `gstreamer1.0-plugins-good`, `gstreamer1.0-plugins-bad`,
     `libwayland-client0`, `libwayland-egl1`, `libegl1`
   - rpm (approx): `gstreamer1`, `gstreamer1-plugins-base`,
     `gstreamer1-plugins-good`, `gstreamer1-plugins-bad-free`,
     `libwayland-client`, `libwayland-egl`, `libglvnd-egl`
   - Recommended/optdepends for extra codecs (ugly/libav) as Suggests.

**Non-goals:** no AUR changes (separate manual protocol per CLAUDE.md), no
mpvpaper conflicts/provides, no new build dependencies.

**Verification:** `workflow_dispatch` dry run must go green on all three matrix
legs before tagging a release; `dpkg -I`/`rpm -qpi` inspection of artifacts;
install test in clean containers (`apt install ./…deb`, `dnf install ./…rpm`)
resolving deps successfully.

## Constraints (repo conventions)

- Modified code blocks carry `// CHANGED YYYY-MM-DD - what - Problem: why`.
- No AI/agent attribution in code, comments, or commit messages.
- Runtime dependency set stays minimal — no new deps without explicit approval.
- Each PR independently revertible; PR 2 depends on PR 1.
