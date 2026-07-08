# gSlapper Perf + Packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Two performance PRs (frame-path allocation elimination, PBO async texture upload) and one CI PR (auto-built deb/rpm release packages), per the approved spec `docs/superpowers/specs/2026-07-08-perf-and-packaging-design.md`.

**Architecture:** PR 1 replaces the per-frame `g_memdup2` copy in `buffer_probe()` with retained, mapped `GstBuffer` references and caches caps parsing. PR 2 (branched on PR 1) routes texture uploads through double-buffered `GL_PIXEL_UNPACK_BUFFER`s with a direct-upload fallback. PR 3 adds `nfpm.yaml` + `.github/workflows/release.yml` with a 3-distro container matrix, modeled on sysc-greet's release workflow.

**Tech Stack:** C, GStreamer 1.x, OpenGL 3.3 (glad), Wayland/EGL, Meson/Ninja, GitHub Actions, nfpm.

## Global Constraints

- Modified code blocks carry `// CHANGED 2026-07-08 - <what> - Problem: <why>` labels (CLAUDE.md).
- NO AI/agent attribution in code, comments, or commit messages (CLAUDE.md).
- NO new build or runtime dependencies.
- Each PR on its own branch; PR 2 branches from PR 1's branch; PR 3 branches from `master`.
- Build check: `ninja -C build` from repo root. Basic test: `./tests/test_basic.sh`.
- Repo: `Nomadcxx/gSlapper`, base branch `master`.

---

### Task 1: PR 1 — Frame path: retained buffers + caps caching

**Files:**
- Modify: `src/main.c:101-108` (struct), `src/main.c:~230` (helper decl area), `src/main.c:360-366` (exit_cleanup), `src/main.c:1049-1052` (render consume), `src/main.c:1693-1762` (buffer_probe), `src/main.c:2471-2478` (cache restore, video path), `src/main.c:2660-2667` (cache restore, image path)

**Interfaces:**
- Produces: `static void release_video_frame_locked(void)` — releases the current frame regardless of origin; caller MUST hold `video_mutex`. New struct fields `video_frame_data.buffer` (`GstBuffer *`) and `video_frame_data.map` (`GstMapInfo`). Invariant: `buffer != NULL` ⇒ `data == map.data` (borrowed); `buffer == NULL && data != NULL` ⇒ `data` is heap-owned (`g_free`able, cache-restore path only).

- [ ] **Step 1: Create branch**

```bash
cd /home/nomadx/Documents/GitHub/gSlapper
git checkout master && git checkout -b perf/frame-path
```

- [ ] **Step 2: Extend `video_frame_data` struct** (src/main.c:101-108)

```c
// Video frame data for thread-safe texture updates
static struct {
    gboolean has_new_frame;
    gint width, height;
    gpointer data;
    gsize size;
    gint last_width, last_height; // Track dimension changes for efficient updates
    // CHANGED 2026-07-08 - Hold a mapped GstBuffer ref instead of a heap copy - Problem: g_memdup2 copied every frame (~14-33MB) on the hot path
    GstBuffer *buffer;   // non-NULL when data borrows from a mapped GStreamer buffer
    GstMapInfo map;      // valid while buffer is non-NULL
} video_frame_data = {0};
```

- [ ] **Step 3: Add release helper** — place directly after the struct definition:

```c
// CHANGED 2026-07-08 - Single ownership-aware release point for the current frame - Problem: five call sites freed frame data with g_free; buffer-backed frames need unmap+unref instead
// Caller must hold video_mutex.
static void release_video_frame_locked(void) {
    if (video_frame_data.buffer) {
        gst_buffer_unmap(video_frame_data.buffer, &video_frame_data.map);
        gst_buffer_unref(video_frame_data.buffer);
        video_frame_data.buffer = NULL;
    } else if (video_frame_data.data) {
        g_free(video_frame_data.data);
    }
    video_frame_data.data = NULL;
    video_frame_data.has_new_frame = FALSE;
}
```

(All existing setters assign `has_new_frame = TRUE` after replacing the frame, so the helper resetting it is safe at every site.)

- [ ] **Step 4: Rewrite `buffer_probe()`** (src/main.c:1693-1762) — caps cached by pointer compare (we hold a ref on `cached_caps`, so no ABA), buffer ref'd + mapped instead of copied, mutex held only for the swap:

```c
static GstPadProbeReturn buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    // CHANGED 2026-07-08 - Cache negotiated caps, re-parse only on renegotiation; retain mapped buffer ref instead of g_memdup2 copy; shrink mutex hold to the pointer swap - Problem: per-frame caps query/strcmp, full-frame malloc+memcpy+free, and long mutex hold contended with render()
    static GstCaps *cached_caps = NULL;
    static gboolean cached_is_rgba = FALSE;
    static gint cached_width = 0, cached_height = 0;

    if (!buffer)
        return GST_PAD_PROBE_OK;

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
        return GST_PAD_PROBE_OK;

    if (caps != cached_caps) {
        GstStructure *structure = gst_caps_get_structure(caps, 0);
        const gchar *format = gst_structure_get_string(structure, "format");
        cached_is_rgba = (format && strcmp(format, "RGBA") == 0);
        cached_width = 0;
        cached_height = 0;
        if (cached_is_rgba) {
            gst_structure_get_int(structure, "width", &cached_width);
            gst_structure_get_int(structure, "height", &cached_height);
            if (VERBOSE == 2)
                cflp_info("Caps negotiated: RGBA %dx%d", cached_width, cached_height);
        } else if (VERBOSE == 2) {
            cflp_info("Frame format is %s, not RGBA", format ? format : "unknown");
        }
        if (cached_caps)
            gst_caps_unref(cached_caps);
        cached_caps = caps; // keep this query's ref as the cache
    } else {
        gst_caps_unref(caps);
    }

    if (cached_is_rgba && cached_width > 0 && cached_height > 0) {
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            gst_buffer_ref(buffer);

            pthread_mutex_lock(&video_mutex);
            release_video_frame_locked();
            video_frame_data.buffer = buffer;
            video_frame_data.map = map;
            video_frame_data.data = map.data;
            video_frame_data.size = map.size;
            video_frame_data.width = cached_width;
            video_frame_data.height = cached_height;
            video_frame_data.has_new_frame = TRUE;
            if (is_image_mode)
                image_frame_captured = true;
            pthread_mutex_unlock(&video_mutex);

            if (VERBOSE == 2) {
                static int frame_count = 0;
                frame_count++;
                if (frame_count % 100 == 0)
                    cflp_info("Captured %dx%d RGBA frame for texture update (frame %d)", cached_width, cached_height, frame_count);
            }

            // Trigger render via wakeup pipe to ensure thread safety
            if (write(wakeup_pipe[1], "f", 1) == -1) {
                if (VERBOSE)
                    cflp_warning("Failed to write to wakeup pipe");
            }
        }
    }

    return GST_PAD_PROBE_OK;
}
```

- [ ] **Step 5: Update render() consume site** (src/main.c:1049-1052; `video_mutex` is already held there — render locks at 1011 and unlocks at 1189). Replace:

```c
        // Free frame data and reset flag
        g_free(video_frame_data.data);
        video_frame_data.data = NULL;
        video_frame_data.has_new_frame = FALSE;
```

with:

```c
        // CHANGED 2026-07-08 - Release mapped buffer ref instead of freeing heap copy - Problem: hot-path frames are now borrowed from GStreamer, not owned allocations
        release_video_frame_locked();
```

- [ ] **Step 6: Update exit_cleanup()** (src/main.c:360-366). Replace the body between lock/unlock with the helper:

```c
    // Clean up video frame data
    pthread_mutex_lock(&video_mutex);
    // CHANGED 2026-07-08 - Ownership-aware release (unmap/unref or g_free) - Problem: frame may be a mapped GstBuffer now
    release_video_frame_locked();
    pthread_mutex_unlock(&video_mutex);
```

- [ ] **Step 7: Update both cache-restore sites** (src/main.c:2471-2474 and 2660-2663). At each, replace:

```c
        if (video_frame_data.data) {
            g_free(video_frame_data.data);
        }
        video_frame_data.data = g_memdup2(cached->data, cached->size);
```

with:

```c
        // CHANGED 2026-07-08 - Ownership-aware release before installing heap-owned cache copy - Problem: previous frame may be a mapped GstBuffer
        release_video_frame_locked();
        video_frame_data.data = g_memdup2(cached->data, cached->size);
```

(`release_video_frame_locked()` leaves `buffer == NULL`, so the installed cache copy is correctly heap-owned. Both sites already hold `video_mutex` — verify while editing; if a site does not, wrap it.)

- [ ] **Step 8: Build**

Run: `ninja -C build`
Expected: success, no new warnings.

- [ ] **Step 9: Basic tests**

Run: `./tests/test_basic.sh`
Expected: PASS (same result as on master).

- [ ] **Step 10: Runtime verification (video + image + cache switch)**

```bash
timeout 15 ./build/gslapper -v '*' /path/to/test/video.mp4; echo "exit: $?"
```
Expected: video visible on both monitors, no errors, exit 124 (timeout). Repeat with an image file, and exercise IPC layer/wallpaper switching if a test asset is handy. Watch RSS: `ps -o rss= -C gslapper` stays flat over the run.

- [ ] **Step 11: Commit**

```bash
git add src/main.c
git commit -m "perf: eliminate per-frame allocation and caps parsing in frame path

Retain mapped GstBuffer references instead of g_memdup2-copying every
decoded frame (~14-33MB per frame at 30-60fps), cache negotiated caps so
format parsing runs only on renegotiation, and shrink the video_mutex
critical section in buffer_probe to the pointer swap."
```

- [ ] **Step 12: Push and open PR**

```bash
git push -u origin perf/frame-path
gh pr create --base master --title "perf: eliminate per-frame allocation and caps parsing in frame path" --body "<summary of the above + benchmark instructions>"
```

---

### Task 2: PR 2 — Double-buffered PBO texture uploads

**Files:**
- Modify: `src/main.c:93-99` (texture_manager struct), after `get_texture_for_dimensions()` (~line 453, new upload helper), `src/main.c:1037-1042` (render upload site), `src/main.c:404-413` (cleanup_texture_manager)

**Interfaces:**
- Consumes: Task 1's `video_frame_data.data/.size` (mapped buffer or heap copy — helper takes plain pointer+size, works with both).
- Produces: `static void upload_frame_to_texture(GLuint texture, const void *data, gsize size, int width, int height)` — must be called with a current EGL context; binds/unbinds its own GL state.

- [ ] **Step 1: Create branch off PR 1**

```bash
git checkout perf/frame-path && git checkout -b perf/pbo-upload
```

- [ ] **Step 2: Extend texture_manager struct** (src/main.c:93-99):

```c
// Smart texture management to reduce reallocations
static struct {
    GLuint texture;
    int current_width;
    int current_height;
    gboolean initialized;
    // CHANGED 2026-07-08 - Double-buffered pixel unpack buffers for async uploads - Problem: glTexSubImage2D from client memory blocks the render thread every frame
    GLuint pbo[2];
    int pbo_index;
    gboolean pbo_disabled; // fall back to direct uploads permanently on failure
} texture_manager = {0};
```

- [ ] **Step 3: Add upload helper** after `get_texture_for_dimensions()` (~line 453):

```c
// CHANGED 2026-07-08 - Upload via ping-ponged PBOs so the driver DMAs frame N-1 while we write frame N - Problem: synchronous client-memory uploads stalled rendering
static void upload_frame_to_texture(GLuint texture, const void *data, gsize size, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, texture);

    if (!texture_manager.pbo_disabled) {
        if (texture_manager.pbo[0] == 0) {
            while (glGetError() != GL_NO_ERROR); // clear stale errors
            glGenBuffers(2, texture_manager.pbo);
            if (texture_manager.pbo[0] == 0 || glGetError() != GL_NO_ERROR) {
                texture_manager.pbo_disabled = TRUE;
                cflp_warning("PBO creation failed, using direct texture uploads");
            } else if (VERBOSE) {
                cflp_info("Using double-buffered PBO texture uploads");
            }
        }
        if (!texture_manager.pbo_disabled) {
            texture_manager.pbo_index ^= 1;
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, texture_manager.pbo[texture_manager.pbo_index]);
            // Orphan the buffer so mapping never syncs against in-flight DMA
            glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_STREAM_DRAW);
            void *dst = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, size,
                                         GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            if (dst) {
                memcpy(dst, data, size);
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                                GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                glBindTexture(GL_TEXTURE_2D, 0);
                return;
            }
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            texture_manager.pbo_disabled = TRUE;
            cflp_warning("PBO map failed, falling back to direct texture uploads");
        }
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}
```

- [ ] **Step 4: Use helper in render()** (src/main.c:1037-1042). Replace:

```c
        GLuint current_texture = get_texture_for_dimensions(video_frame_data.width, video_frame_data.height);
        glBindTexture(GL_TEXTURE_2D, current_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, video_frame_data.width, video_frame_data.height, 
                        GL_RGBA, GL_UNSIGNED_BYTE, video_frame_data.data);
        glBindTexture(GL_TEXTURE_2D, 0);
```

with:

```c
        // CHANGED 2026-07-08 - Route upload through PBO helper - Problem: direct upload stalled the render thread
        GLuint current_texture = get_texture_for_dimensions(video_frame_data.width, video_frame_data.height);
        upload_frame_to_texture(current_texture, video_frame_data.data, video_frame_data.size,
                                video_frame_data.width, video_frame_data.height);
```

- [ ] **Step 5: Clean up PBOs** in `cleanup_texture_manager()` (src/main.c:404-413):

```c
static void cleanup_texture_manager() {
    if (texture_manager.initialized) {
        glDeleteTextures(1, &texture_manager.texture);
        texture_manager.texture = 0;
        texture_manager.initialized = FALSE;
        if (VERBOSE)
            cflp_info("Cleaned up texture manager");
    }
    // CHANGED 2026-07-08 - Delete upload PBOs - Problem: new GL objects need cleanup
    if (texture_manager.pbo[0] != 0) {
        glDeleteBuffers(2, texture_manager.pbo);
        texture_manager.pbo[0] = 0;
        texture_manager.pbo[1] = 0;
    }
}
```

- [ ] **Step 6: Build + tests**

Run: `ninja -C build && ./tests/test_basic.sh`
Expected: build success, tests PASS. Confirm `glad.h` exposes `glMapBufferRange`/`glBindBuffer` (GL 3.0+ loader — `grep -c glMapBufferRange inc/glad/glad.h` ≥ 1).

- [ ] **Step 7: Runtime verification**

Same as Task 1 Step 10, plus: `-v` log must show "Using double-buffered PBO texture uploads"; test a transition (wallpaper switch) since `render_transition()` shares the texture path; verify no visual tearing/corruption on both monitors.

- [ ] **Step 8: Commit, push, PR**

```bash
git add src/main.c
git commit -m "perf: upload frames through double-buffered PBOs

Ping-pong two GL_PIXEL_UNPACK_BUFFERs with orphaning so texture DMA for
the previous frame overlaps CPU writes for the current one, instead of
blocking in glTexSubImage2D. Falls back to direct uploads permanently if
PBO creation or mapping fails."
git push -u origin perf/pbo-upload
gh pr create --base perf/frame-path --title "perf: upload frames through double-buffered PBOs" --body "<summary + note: rebase onto master after PR 1 merges>"
```

---

### Task 3: PR 3 — Release packaging workflow (deb/rpm)

**Files:**
- Create: `nfpm.yaml`
- Create: `.github/workflows/release.yml`

**Interfaces:**
- Consumes: `meson setup build --prefix=/usr` build outputs `build/gslapper`, `build/gslapper-holder`; repo files `gslapper.service`, `gslapper@.service`, `LICENSE`, `README.md`. Version comes from git tag (`v*` → `${VERSION}` env, matching sysc-greet).
- Produces: release assets `gslapper_${VER}_ubuntu24.04_amd64.deb`, `gslapper_${VER}_debian13_amd64.deb`, `gslapper-${VER}-1.fedora42.x86_64.rpm`, `SHA256SUMS`.

- [ ] **Step 1: Create branch**

```bash
git checkout master && git checkout -b ci/release-packages
```

- [ ] **Step 2: Write `nfpm.yaml`** (ends with a bare `depends:` key; CI appends per-distro entries — same pattern as sysc-greet's `nfpm-cagebreak.yaml`):

```yaml
name: gslapper
arch: amd64
platform: linux
version: ${VERSION}
section: misc
priority: optional
maintainer: Nomadcxx <noovie@gmail.com>
description: |
  Wallpaper utility for Wayland with video/image support and instant
  switching via RAM cache. GStreamer-based replacement for mpvpaper.
vendor: Nomadcxx
homepage: https://github.com/Nomadcxx/gSlapper
license: MIT

contents:
  - src: ./build/gslapper
    dst: /usr/bin/gslapper
    file_info:
      mode: 0755
  - src: ./build/gslapper-holder
    dst: /usr/bin/gslapper-holder
    file_info:
      mode: 0755
  - src: ./gslapper.service
    dst: /usr/lib/systemd/user/gslapper.service
    file_info:
      mode: 0644
  - src: ./gslapper@.service
    dst: /usr/lib/systemd/user/gslapper@.service
    file_info:
      mode: 0644
  - src: ./LICENSE
    dst: /usr/share/licenses/gslapper/LICENSE
    file_info:
      mode: 0644
  - src: ./README.md
    dst: /usr/share/doc/gslapper/README.md
    file_info:
      mode: 0644

depends:
```

- [ ] **Step 3: Write `.github/workflows/release.yml`**:

```yaml
name: Release

on:
  push:
    tags:
      - 'v*'
  # Dry run: builds everything, skips release creation
  workflow_dispatch:

permissions:
  contents: write

jobs:
  package:
    runs-on: ubuntu-latest
    container: ${{ matrix.image }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - image: "ubuntu:24.04"
            distro: ubuntu24.04
            pkgmgr: apt
            packager: deb
            depends: "libgstreamer1.0-0 libgstreamer-plugins-base1.0-0 libgstreamer-gl1.0-0 gstreamer1.0-plugins-good gstreamer1.0-plugins-bad libwayland-client0 libwayland-egl1 libegl1"
          - image: "debian:13"        # also serves Ubuntu 25.x
            distro: debian13
            pkgmgr: apt
            packager: deb
            depends: "libgstreamer1.0-0 libgstreamer-plugins-base1.0-0 libgstreamer-gl1.0-0 gstreamer1.0-plugins-good gstreamer1.0-plugins-bad libwayland-client0 libwayland-egl1 libegl1"
          - image: "fedora:42"
            distro: fedora42
            pkgmgr: dnf
            packager: rpm
            depends: "gstreamer1 gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugins-bad-free libwayland-client libwayland-egl libglvnd-egl"
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Get version
        id: get_version
        run: |
          if [[ "$GITHUB_REF" == refs/tags/v* ]]; then
            VERSION=${GITHUB_REF#refs/tags/v}
          else
            VERSION=0.0.0-dev
          fi
          echo "VERSION=$VERSION" >> $GITHUB_OUTPUT
        shell: bash

      - name: Install build dependencies (apt)
        if: matrix.pkgmgr == 'apt'
        run: |
          export DEBIAN_FRONTEND=noninteractive
          apt-get update -qq
          apt-get install -y -qq --no-install-recommends \
            curl ca-certificates build-essential meson ninja-build pkg-config \
            wayland-protocols libwayland-dev libegl-dev \
            libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libsystemd-dev

      - name: Install build dependencies (dnf)
        if: matrix.pkgmgr == 'dnf'
        run: |
          dnf install -y -q curl gcc meson ninja-build pkgconf-pkg-config \
            wayland-devel wayland-protocols-devel libglvnd-devel \
            gstreamer1-devel gstreamer1-plugins-base-devel systemd-devel

      - name: Build
        run: |
          meson setup build --prefix=/usr --buildtype=release
          ninja -C build

      - name: Smoke test
        run: |
          env -u WAYLAND_DISPLAY ./build/gslapper --help > /dev/null
          env -u WAYLAND_DISPLAY ./build/gslapper-holder --help > /dev/null || true

      - name: Install nfpm
        run: |
          curl -fsSL "https://github.com/goreleaser/nfpm/releases/download/v2.47.0/nfpm_2.47.0_Linux_x86_64.tar.gz" \
            | tar -xz -C /usr/local/bin nfpm

      - name: Package
        env:
          VERSION: ${{ steps.get_version.outputs.VERSION }}
          DEPENDS: ${{ matrix.depends }}
        run: |
          # nfpm.yaml ends with a bare `depends:` key; append per-distro runtime libs
          for dep in $DEPENDS; do
            printf '  - "%s"\n' "$dep" >> nfpm.yaml
          done
          if [ "${{ matrix.packager }}" = "deb" ]; then
            PKGFILE="gslapper_${VERSION}_${{ matrix.distro }}_amd64.deb"
          else
            PKGFILE="gslapper-${VERSION}-1.${{ matrix.distro }}.x86_64.rpm"
          fi
          nfpm package --config nfpm.yaml --packager ${{ matrix.packager }} --target "$PKGFILE"
          echo "PKGFILE=$PKGFILE" >> "$GITHUB_ENV"

      - name: Verify package installs
        run: |
          if [ "${{ matrix.pkgmgr }}" = "apt" ]; then
            apt-get install -y "./$PKGFILE"
          else
            dnf install -y "./$PKGFILE"
          fi
          env -u WAYLAND_DISPLAY /usr/bin/gslapper --help > /dev/null

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: gslapper-${{ matrix.distro }}
          path: "*.deb\n*.rpm"

  release:
    if: startsWith(github.ref, 'refs/tags/v')
    needs: [package]
    runs-on: ubuntu-latest
    steps:
      - name: Download artifacts
        uses: actions/download-artifact@v4
        with:
          merge-multiple: true

      - name: Generate checksums
        run: |
          sha256sum *.deb *.rpm > SHA256SUMS

      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            *.deb
            *.rpm
            SHA256SUMS
          generate_release_notes: true
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

(Note: `upload-artifact` `path` needs newline-separated globs — write as a YAML block scalar, not the escaped string above, when creating the real file.)

- [ ] **Step 4: Validate locally.** Lint YAML and dry-run nfpm on the local build:

```bash
ninja -C build
VERSION=0.0.0-local sh -c 'cp nfpm.yaml /tmp/nfpm-test.yaml && printf "  - \"gstreamer\"\n" >> /tmp/nfpm-test.yaml && nfpm package --config /tmp/nfpm-test.yaml --packager deb --target /tmp/gslapper-test.deb' \
  || echo "install nfpm locally first: go install github.com/goreleaser/nfpm/v2/cmd/nfpm@latest or download the release binary"
dpkg-deb -I /tmp/gslapper-test.deb && dpkg-deb -c /tmp/gslapper-test.deb
```

Expected: control shows name/version/depends; contents list the two binaries, two user units, LICENSE, README. Note `nfpm` expands `${VERSION}` from the environment.

- [ ] **Step 5: Verify runtime dep names against the built binary** (done in CI legs on first dry run): after the workflow's first `workflow_dispatch` run, check each leg's log — if `ldd build/gslapper` shows libs not covered by the `depends` list (or names differ per distro), fix the matrix entries. The `Verify package installs` step catches wrong names by failing dependency resolution.

- [ ] **Step 6: Commit, push, PR**

```bash
git add nfpm.yaml .github/workflows/release.yml
git commit -m "ci: build Debian/Ubuntu and Fedora packages on release

Adds an nfpm config and a tag-triggered workflow that builds gslapper in
ubuntu:24.04, debian:13, and fedora:42 containers, packages deb/rpm with
per-distro runtime dependencies, verifies each package installs cleanly,
and attaches artifacts plus SHA256SUMS to the GitHub release.
workflow_dispatch runs everything as a dry run without releasing."
git push -u origin ci/release-packages
gh pr create --base master --title "ci: build Debian/Ubuntu and Fedora packages on release" --body "<summary + dry-run instructions>"
```

- [ ] **Step 7: Trigger dry run** (after PR is pushed):

```bash
gh workflow run release.yml --ref ci/release-packages
gh run watch
```

Expected: all three matrix legs green. Fix dep lists per Step 5 if a leg fails, amend, re-run.
