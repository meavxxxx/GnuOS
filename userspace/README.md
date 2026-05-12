# Userspace Layout

Userspace bring-up tree for GNU OS.

## Subdirectories

- `libc/`: libc and dynamic-loader scaffolding (`ld-gnuos.so.1`, `libc.so.6` stub)
- `init/`: minimal init/smoke program targets
- `coreutils/`: placeholder for GNU Coreutils porting phase
- `bash/`: placeholder for GNU Bash porting phase
- `drivers/`: placeholder for future userspace driver/services model

## Current state

- Stage0 loader and libc ABI scaffolding are actively developed under `userspace/libc/`.
- Smoke targets are built through `make userspace`.
