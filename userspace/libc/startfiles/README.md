# GNU OS start files

This directory contains minimal startup files for userspace ABI bring-up:

- `crt0.S` provides `_start`, extracts `argc/argv/envp`, and calls `main`.
- `crti.S` provides `_init`/`_fini` prologues.
- `crtn.S` provides `_init`/`_fini` epilogues.

Current status:

- x86_64 scaffolding is implemented in `x86_64/`.
- These files are intended for early libc/glibc porting work.
- They are built by `make userspace` and smoke-linked with `userspace/init/init_minimal.c`.
