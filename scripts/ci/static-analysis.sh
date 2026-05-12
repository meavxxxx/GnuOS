#!/usr/bin/env bash
set -euo pipefail

ARCH="${ARCH:-x86_64}"
TARGET="${TARGET:-x86_64-linux-gnu}"
BUILD_DIR="${BUILD_DIR:-build/${ARCH}}"
COMPILE_DB="${BUILD_DIR}/compile_commands.json"

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "error: required tool '$1' is not installed" >&2
        exit 1
    fi
}

require_tool make
require_tool bear
require_tool cppcheck
require_tool clang-tidy

echo "[static-analysis] generating compile database via bear..."
make ARCH="$ARCH" TARGET="$TARGET" clean
mkdir -p "$(dirname "$COMPILE_DB")"
bear --output "$COMPILE_DB" -- make ARCH="$ARCH" TARGET="$TARGET" kernel userspace >/dev/null

echo "[static-analysis] running cppcheck..."
cppcheck \
    --enable=warning,performance,portability \
    --std=c11 \
    --language=c \
    --error-exitcode=1 \
    --quiet \
    --suppress=missingInclude \
    --suppress=missingIncludeSystem \
    -Iinclude \
    -Ikernel/include \
    -Iuserspace/libc/include \
    kernel \
    userspace

echo "[static-analysis] running clang-tidy (clang-analyzer checks)..."
clang-tidy \
    -p "$BUILD_DIR" \
    -checks='-*,clang-analyzer-*' \
    kernel/init/kmain.c \
    kernel/init/panic.c \
    kernel/mm/pmm.c \
    kernel/mm/vmm.c \
    kernel/security/seccomp.c \
    kernel/syscall/syscall.c \
    userspace/libc/ldso/ldso_dlfcn.c \
    userspace/libc/ldso/ldso_elf.c \
    userspace/libc/ldso/x86_64/ldso_bootstrap.c \
    userspace/libc/ldso/x86_64/libc_stub.c \
    userspace/init/init_minimal.c \
    --quiet

echo "[static-analysis] done."
