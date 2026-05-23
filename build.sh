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
echo "[+] Built artifacts in $BUILD_DIR :"
echo "       armkit              — standalone PIE executable"
echo "       payload.elf         — relocation-free blob (ELF)"
echo "       armkit_payload.bin  — flat blob produced by objcopy"
echo "       loader              — host program: mmap+memcpy+mprotect+jump"
echo ""

# Inspect the payload to confirm no runtime relocations remain.
for re in aarch64-linux-android-readelf aarch64-linux-gnu-readelf llvm-readelf readelf; do
    if command -v "$re" &>/dev/null; then
        echo "[*] payload.elf sections (via $re):"
        "$re" -S "$BUILD_DIR/payload.elf" 2>/dev/null \
            | grep -E '\.text|\.rodata|\.data|\.rela|\.dynamic' || true
        echo ""
        echo "[*] payload.elf relocations (should be EMPTY):"
        "$re" -r "$BUILD_DIR/payload.elf" 2>/dev/null | head -5 || true
        break
    fi
done

ls -la "$BUILD_DIR/armkit_payload.bin" 2>/dev/null || true
echo ""
echo "[*] To run the standalone PIE on a device:"
echo "    adb push $BUILD_DIR/armkit /data/local/tmp/"
echo "    adb shell chmod +x /data/local/tmp/armkit"
echo "    adb shell /data/local/tmp/armkit"
echo ""
echo "[*] To run the loader+blob on a device:"
echo "    adb push $BUILD_DIR/loader              /data/local/tmp/"
echo "    adb push $BUILD_DIR/armkit_payload.bin  /data/local/tmp/"
echo "    adb shell chmod +x /data/local/tmp/loader"
echo "    adb shell /data/local/tmp/loader /data/local/tmp/armkit_payload.bin"
