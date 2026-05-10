#!/usr/bin/env bash
set -euo pipefail

ARCH="${ARCH:-x86_64}"

make ARCH="$ARCH" clean
make ARCH="$ARCH" kernel

