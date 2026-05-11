# GNU OS dynamic loader scaffold

This directory hosts early dynamic-linker scaffolding for userspace ABI bring-up.

- `x86_64/ldso_start.S` provides a stage-0 entrypoint for `ld-gnuos.so.1`.
- `ldso_elf.c` + `ldso_elf.h` classify `PT_LOAD`, `PT_DYNAMIC`, `PT_GNU_RELRO`,
  parse `PT_DYNAMIC`, apply `RELA` relocations for `R_X86_64_*`, and run
  `DT_INIT`/`DT_INIT_ARRAY` initialization sequence.
- `ldso_dlfcn.c` + `ldso_dlfcn.h` provide stage-0 `dlopen`/`dlsym`/`dlclose`/`dlerror`
  with pre-registered object registry support.
- `x86_64/ldso_bootstrap.c` parses auxv (`AT_PHDR/AT_PHENT/AT_PHNUM`) and records
  stage-0 program-header layout metadata, computes load bias, runs relocation pass,
  and applies `LD_PRELOAD` entries from env for pre-registered objects.
  Symbol resolver now uses the stage-0 object registry and built-in symbol table.
- `x86_64/libc_stub.c` provides a minimal shared `libc.so.6` stub used by smoke tests.

Current status:

- `make userspace` builds `ld-gnuos.so.1` and a smoke executable with
  `PT_INTERP=/lib/ld-gnuos.so.1`.
- Dynamic smoke executable includes both `DT_INIT` and `DT_INIT_ARRAY`.
- `userspace/libc/include/dlfcn.h` is present for userspace ABI scaffolding.
- `LD_PRELOAD` stage-0 support currently resolves pre-registered object names only.
- `libc.so.6` stub now exports `dl_iterate_phdr` and `backtrace` scaffolding APIs.
- The sysroot installer places:
  - loader at `/lib/ld-gnuos.so.1`
  - stub libc at `/usr/lib/libc.so.6` (and `/usr/lib/libc.so` for link-time lookup)
