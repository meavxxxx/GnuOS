#!/usr/bin/env bash
set -euo pipefail

COVERAGE_DIR="${COVERAGE_DIR:-build/coverage}"

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "error: required tool '$1' is not installed" >&2
        exit 1
    fi
}

require_tool gcc
require_tool lcov

mkdir -p "$COVERAGE_DIR"

echo "[coverage] building unit test with gcov instrumentation..."
gcc \
    --coverage \
    -O0 \
    -g \
    -std=gnu17 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Iuserspace/libc/ldso \
    -c userspace/libc/ldso/ldso_dlfcn.c \
    -o "$COVERAGE_DIR/ldso_dlfcn.o"

gcc \
    --coverage \
    -O0 \
    -g \
    -std=gnu17 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Iuserspace/libc/ldso \
    -c tests/unit/ldso_dlfcn_test.c \
    -o "$COVERAGE_DIR/ldso_dlfcn_test.o"

gcc \
    --coverage \
    "$COVERAGE_DIR/ldso_dlfcn.o" \
    "$COVERAGE_DIR/ldso_dlfcn_test.o" \
    -o "$COVERAGE_DIR/ldso_dlfcn_test"

echo "[coverage] running unit test..."
"$COVERAGE_DIR/ldso_dlfcn_test"

echo "[coverage] collecting lcov report..."
lcov --capture --directory "$COVERAGE_DIR" --output-file "$COVERAGE_DIR/coverage.info"
lcov --remove "$COVERAGE_DIR/coverage.info" "/usr/*" "tests/*" \
    --output-file "$COVERAGE_DIR/coverage.filtered.info"

echo "[coverage] report: $COVERAGE_DIR/coverage.filtered.info"
