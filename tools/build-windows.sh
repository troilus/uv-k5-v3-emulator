#!/usr/bin/env bash
# build-windows.sh -- build the UV-K5 V3 emulator for Windows.
#
# Run this inside an MSYS2 UCRT64 shell:
#
#   pacman -S base-devel mingw-w64-ucrt-x86_64-toolchain \
#             mingw-w64-ucrt-x86_64-glib2 mingw-w64-ucrt-x86_64-pixman \
#             mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-meson \
#             mingw-w64-ucrt-x86_64-python
#
#   cd /path/to/uv-k5-v3-emulator
#   bash tools/build-windows.sh
#
# Produces: release/uvk5.exe  +  release/flash.img
set -euo pipefail

QEMU_VERSION=7.2.0
QEMU_URL="https://download.qemu.org/qemu-${QEMU_VERSION}.tar.xz"
QEMU_DIR="qemu-${QEMU_VERSION}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$REPO_DIR/qemu-build"
RELEASE_DIR="$REPO_DIR/release"

echo "=== UV-K5 V3 Windows build ==="
echo "QEMU source : $QEMU_DIR"
echo "Build dir   : $BUILD_DIR"
echo "Release dir : $RELEASE_DIR"

# ---- 1. Download QEMU if not present ----
if [ ! -d "$BUILD_DIR/$QEMU_DIR" ]; then
    echo "--- Downloading QEMU $QEMU_VERSION ---"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    curl -L -o "qemu-${QEMU_VERSION}.tar.xz" "$QEMU_URL"
    tar xf "qemu-${QEMU_VERSION}.tar.xz"
    rm -f "qemu-${QEMU_VERSION}.tar.xz"
else
    echo "--- Using existing QEMU source ---"
    cd "$BUILD_DIR"
fi

# ---- 2. Apply patches ----
echo "--- Applying patches ---"
QSRC="$BUILD_DIR/$QEMU_DIR"

# Machine model
cp "$REPO_DIR/qemu/py32f071.c" "$QSRC/hw/arm/"

# Patched SysTick
cp "$REPO_DIR/qemu/armv7m_systick.c.patched" "$QSRC/hw/timer/armv7m_systick.c"
cp "$REPO_DIR/qemu/armv7m_systick.h.patched" "$QSRC/include/hw/timer/armv7m_systick.h"

# Kconfig additions (idempotent)
if ! grep -q 'UVK5_V3' "$QSRC/hw/arm/Kconfig"; then
    cat >> "$QSRC/hw/arm/Kconfig" << 'EOF'

config UVK5_V3
    bool
    default y
    depends on TCG && ARM
    select PY32F071_SOC

config PY32F071_SOC
    bool
    select ARM_V7M
    select UNIMP
EOF
fi

# meson.build registration (idempotent)
if ! grep -q 'UVK5_V3' "$QSRC/hw/arm/meson.build"; then
    sed -i "/arm_ss.add(when: 'CONFIG_STM32VLDISCOVERY'/a\\
arm_ss.add(when: 'CONFIG_UVK5_V3', if_true: files('py32f071.c'))" \
        "$QSRC/hw/arm/meson.build"
fi

# ---- 3. Configure ----
echo "--- Configuring QEMU ---"
mkdir -p "$QSRC/build"
cd "$QSRC/build"

if [ ! -f build.ninja ]; then
    ../configure \
        --target-list=arm-softmmu \
        --disable-docs \
        --disable-tools \
        --disable-sdl \
        --disable-gtk \
        --disable-vnc \
        --disable-spice \
        --disable-slirp \
        --disable-capstone \
        --disable-libusb
fi

# ---- 4. Build ----
echo "--- Building QEMU ---"
ninja qemu-system-arm

# ---- 5. Package ----
echo "--- Packaging release ---"
mkdir -p "$RELEASE_DIR"
cp arm-softmmu/qemu-system-arm.exe "$RELEASE_DIR/uvk5.exe"
python3 "$REPO_DIR/tools/make_flash.py" --out "$RELEASE_DIR/flash.img"
cp "$REPO_DIR/tools/key.py" "$RELEASE_DIR/"
cp "$REPO_DIR/tools/screenshot.py" "$RELEASE_DIR/"

echo
echo "=== Build complete ==="
echo "EXE  : $RELEASE_DIR/uvk5.exe"
echo "Flash: $RELEASE_DIR/flash.img"
echo
echo "To run: place firmware.elf in $RELEASE_DIR/ and double-click uvk5.exe"
