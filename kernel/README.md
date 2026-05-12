# Kernel Layout

Kernel source tree for GNU OS.

## Main areas

- `arch/`: architecture-specific code (`x86_64` currently active)
- `init/`: early bring-up and bootstrap entrypoints
- `mm/`: physical and virtual memory management
- `sched/`: scheduler and task structures
- `syscall/`: syscall dispatch and fast paths
- `ipc/`: kernel-side IPC primitives
- `drivers/`: early platform and device drivers
- `security/`: capability and seccomp scaffolding
- `fs/`, `net/`: reserved for filesystem/network stack expansion

## Key entrypoints

- Kernel C entry: `kernel/init/kmain.c`
- Panic path: `kernel/init/panic.c`
- Linker script: `kernel/arch/x86_64/linker.ld`

## Build

- `make ARCH=x86_64 kernel`
