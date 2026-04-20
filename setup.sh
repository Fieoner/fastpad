#!/bin/bash
set -e

# Download and extract WCH CH32V307 EVT SDK vendor files
REPO_URL="https://github.com/openwch/ch32v307.git"
TMP_DIR=$(mktemp -d)

echo "Cloning WCH CH32V307 EVT SDK..."
git clone --depth 1 "$REPO_URL" "$TMP_DIR/sdk"

SDK_SRC="$TMP_DIR/sdk/EVT/EXAM/SRC"

echo "Copying vendor files..."
mkdir -p vendor/Core vendor/Debug vendor/Startup vendor/Ld vendor/Peripheral/inc vendor/Peripheral/src

cp "$SDK_SRC/Core/core_riscv.c" vendor/Core/
cp "$SDK_SRC/Core/core_riscv.h" vendor/Core/

cp "$SDK_SRC/Debug/debug.c" vendor/Debug/
cp "$SDK_SRC/Debug/debug.h" vendor/Debug/

cp "$SDK_SRC/Startup/startup_ch32v30x_D8C.S" vendor/Startup/

cp "$SDK_SRC/Ld/Link.ld" vendor/Ld/

cp "$SDK_SRC/Peripheral/inc/"*.h vendor/Peripheral/inc/
cp "$SDK_SRC/Peripheral/src/"*.c vendor/Peripheral/src/

# Copy system_ch32v30x.c from any example User dir
SYSTEM_SRC=$(find "$TMP_DIR/sdk/EVT/EXAM" -name "system_ch32v30x.c" -path "*/User/*" | head -1)
cp "$SYSTEM_SRC" vendor/

echo "Cleaning up..."
rm -rf "$TMP_DIR"

echo "Done! Vendor files are in vendor/"
echo "Run 'make' to build."
