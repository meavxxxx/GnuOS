# GNU OS dynamic loader scaffold

This directory hosts early dynamic-linker scaffolding for userspace ABI bring-up.

- `x86_64/ldso_start.S` provides a stage-0 entrypoint for `ld-gnuos.so.1`.
- `ldso_elf.c` + `ldso_elf.h` classify `PT_LOAD`, `PT_DYNAMIC`, `PT_GNU_RELRO`,
  parse `PT_DYNAMIC`, and apply `RELA` relocations for `R_X86_64_*`.
- `x86_64/ldso_bootstrap.c` parses auxv (`AT_PHDR/AT_PHENT/AT_PHNUM`) and records
  stage-0 program-header layout metadata, computes load bias, and runs relocation pass.
  Current symbol resolver is a bootstrap stub for smoke symbols only.
- `x86_64/libc_stub.c` provides a minimal shared `libc.so.6` stub used by smoke tests.

Current status:

- `make userspace` builds `ld-gnuos.so.1` and a smoke executable with
  `PT_INTERP=/lib/ld-gnuos.so.1`.
- The sysroot installer places:
  - loader at `/lib/ld-gnuos.so.1`
  - stub libc at `/usr/lib/libc.so.6` (and `/usr/lib/libc.so` for link-time lookup)
