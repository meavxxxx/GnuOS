# Scripts Index

This directory contains automation for development, CI, toolchains, and local runtime.

## Subdirectories

- [`scripts/ci/`](ci): CI-oriented checks (`static-analysis`, `coverage`, `fuzz`, wrapper `check.sh`)
- [`scripts/dev/`](dev): local developer setup and helper wrappers for Windows + WSL/Docker
- [`scripts/docker/`](docker): reproducible build container image
- [`scripts/qemu/`](qemu): QEMU launch helpers (legacy ISO and UEFI/OVMF flows)
- [`scripts/toolchain/`](toolchain): cross-toolchain and sysroot install helpers

## Common entrypoints

- Full local setup: `scripts/dev/setup-dev.ps1`
- WSL make wrapper: `scripts/dev/wsl-make.ps1`
- WSL kernel run: `scripts/dev/wsl-run-kernel.ps1`
- CI static analysis: `scripts/ci/static-analysis.sh`
- CI coverage: `scripts/ci/coverage.sh`
- CI fuzz smoke: `scripts/ci/fuzz.sh`
