#!/usr/bin/env bash
set -euo pipefail

FUZZ_DIR="${FUZZ_DIR:-build/fuzz}"

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "error: required tool '$1' is not installed" >&2
        exit 1
    fi
}

require_tool clang

mkdir -p "$FUZZ_DIR"

echo "[fuzz] building libFuzzer harness..."
clang \
    -fsanitize=fuzzer,address \
    -fno-omit-frame-pointer \
    -O1 \
    -g \
    -std=gnu17 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -DLDSO_DLFCN_DISABLE_SHIMS \
    -Iuserspace/libc/ldso \
    tests/fuzz/fuzz_ldso_dlfcn.c \
    userspace/libc/ldso/ldso_dlfcn.c \
    -o "$FUZZ_DIR/fuzz_ldso_dlfcn"

echo "[fuzz] running smoke fuzz session..."
"$FUZZ_DIR/fuzz_ldso_dlfcn" -runs=5000 -max_total_time=20

echo "[fuzz] done."
