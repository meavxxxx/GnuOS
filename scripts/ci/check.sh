#!/usr/bin/env bash
set -euo pipefail

ARCH="${ARCH:-x86_64}"

make ARCH="$ARCH" clean
make ARCH="$ARCH" kernel

if [[ "${RUN_STATIC_ANALYSIS:-0}" == "1" ]]; then
    bash scripts/ci/static-analysis.sh
fi

if [[ "${RUN_COVERAGE:-0}" == "1" ]]; then
    bash scripts/ci/coverage.sh
fi

if [[ "${RUN_FUZZ_SMOKE:-0}" == "1" ]]; then
    bash scripts/ci/fuzz.sh
fi
