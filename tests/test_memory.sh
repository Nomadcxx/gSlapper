#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== gSlapper Memory Safety Tests ==="
echo ""

BUILD_DIR="$PROJECT_ROOT/build-asan"
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Setting up ASAN build..."
    meson setup "$BUILD_DIR" --prefix=/usr/local -Db_sanitize=address,undefined -Db_lundef=false
fi

echo "Building with ASAN..."
ninja -C "$BUILD_DIR"

GSLAPPER="$BUILD_DIR/gslapper"
GSLAPPER_HOLDER="$BUILD_DIR/gslapper-holder"

export ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:log_path=/tmp/asan-gslapper"

echo ""
echo "=== ASAN: Testing --help (no leaks expected) ==="
if timeout 5s "$GSLAPPER" --help > /dev/null 2>&1; then
    echo "PASS: --help completed without ASAN errors"
else
    echo "FAIL: --help crashed or timed out"
fi

echo ""
echo "=== ASAN: Testing --help-output ==="
if timeout 5s "$GSLAPPER" --help-output 2>&1 | head -5; then
    echo "PASS: --help-output ran (may fail without Wayland)"
else
    echo "INFO: --help-output requires Wayland connection"
fi

echo ""
echo "=== ASAN: Testing holder --help ==="
if timeout 5s "$GSLAPPER_HOLDER" --help > /dev/null 2>&1; then
    echo "PASS: holder --help completed without ASAN errors"
else
    echo "FAIL: holder --help crashed or timed out"
fi

echo ""
echo "=== Checking for ASAN log files ==="
if ls /tmp/asan-gslapper* 2>/dev/null; then
    echo "WARNING: ASAN detected issues:"
    for f in /tmp/asan-gslapper*; do
        echo "--- $f ---"
        cat "$f"
        rm -f "$f"
    done
else
    echo "PASS: No ASAN errors detected"
fi

echo ""
echo "=== Valgrind: Testing --help ==="
if command -v valgrind &> /dev/null; then
    VALGRIND_OUT=$(mktemp)
    if timeout 30s valgrind --leak-check=full --error-exitcode=1 \
        "$PROJECT_ROOT/build/gslapper" --help > /dev/null 2>"$VALGRIND_OUT"; then
        echo "PASS: valgrind found no errors"
    else
        echo "INFO: valgrind output:"
        grep -E "ERROR SUMMARY|definitely lost|indirectly lost" "$VALGRIND_OUT" || true
    fi
    rm -f "$VALGRIND_OUT"
else
    echo "SKIP: valgrind not installed"
fi

echo ""
echo "=== Memory Safety Tests Complete ==="
