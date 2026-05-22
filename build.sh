#!/usr/bin/env bash
# build.sh — convenience wrapper for building armkit with the Android NDK.
#
# Usage:
#   ./build.sh [--ndk /path/to/ndk] [--api 28] [--clean]
#
# Defaults:
#   NDK   = $ANDROID_NDK_HOME or $ANDROID_NDK or auto-detected
#   API   = 28 (Android 9 = first stable arm64 dynamic loader skip possible)

set -euo pipefail

NDK=""
API=28
CLEAN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ndk)  NDK="$2"; shift 2 ;;
        --api)  API="$2"; shift 2 ;;
        --clean) CLEAN=1; shift ;;
        *) echo "Unknown argument: $1"; exit 1 ;;
    esac
done

# Auto-detect NDK
if [[ -z "$NDK" ]]; then
    for candidate in "$ANDROID_NDK_HOME" "$ANDROID_NDK" \
                     "$HOME/Android/Sdk/ndk-bundle" \
                     "$HOME/Library/Android/sdk/ndk-bundle"; do
        [[ -n "$candidate" && -d "$candidate" ]] && { NDK="$candidate"; break; }
    done
fi

if [[ -z "$NDK" || ! -d "$NDK" ]]; then
    echo "[!] Android NDK not found. Set ANDROID_NDK_HOME or pass --ndk /path"
    exit 1
fi

TOOLCHAIN="$NDK/build/cmake/android.toolchain.cmake"
if [[ ! -f "$TOOLCHAIN" ]]; then
    echo "[!] Toolchain file not found: $TOOLCHAIN"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-arm64"

if [[ "$CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
    echo "[*] Cleaning $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "[*] Configuring (NDK=$NDK, API=$API)"
cmake "$SCRIPT_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM="android-$API" \
    -DANDROID_STL=none \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja

echo "[*] Building"
ninja -v

echo ""
echo "[+] Built: $BUILD_DIR/armkit"
echo ""

# Print ELF header summary if readelf is available
for re in aarch64-linux-android-readelf aarch64-linux-gnu-readelf readelf; do
    if command -v "$re" &>/dev/null; then
        echo "[*] ELF header (via $re):"
        "$re" -h "$BUILD_DIR/armkit" 2>/dev/null | grep -E 'Type|Entry|Flags|Machine' || true
        echo ""
        echo "[*] Program headers:"
        "$re" -l "$BUILD_DIR/armkit" 2>/dev/null | head -40 || true
        echo ""
        echo "[*] Dynamic section (should be empty or absent for static-pie):"
        "$re" -d "$BUILD_DIR/armkit" 2>/dev/null | head -20 || true
        break
    fi
done

echo "[*] To run on a device:"
echo "    adb push $BUILD_DIR/armkit /data/local/tmp/armkit"
echo "    adb shell chmod +x /data/local/tmp/armkit"
echo "    adb shell /data/local/tmp/armkit"
