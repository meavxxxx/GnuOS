#!/usr/bin/env bash
set -euo pipefail

ARCH="${ARCH:-x86_64}"
ISO_PATH="${1:-build/$ARCH/gnuos-$ARCH.iso}"

if [[ ! -f "$ISO_PATH" ]]; then
    echo "ISO not found: $ISO_PATH"
    echo "Run: make ARCH=$ARCH image"
    exit 1
fi

qemu-system-x86_64 -cdrom "$ISO_PATH" -serial stdio

