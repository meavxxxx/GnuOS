# GNU OS Target Bootstrap

This document describes the current userspace target scaffold for `x86_64-gnuos`.

## What is implemented

- Minimal startup files (`crt0.o`, `crti.o`, `crtn.o`) for x86_64.
- Minimal public libc header set in `userspace/libc/include`.
- Sysroot install pipeline:
  - `scripts/toolchain/install-gnuos-sysroot.sh`
  - output: `build/x86_64/sysroot/x86_64-gnuos`
- Smoke userspace ELF build against the generated sysroot:
  - `build/x86_64/userspace/init/init_minimal.elf`

## Commands

```bash
make ARCH=x86_64 userspace-startfiles
make ARCH=x86_64 userspace-sysroot
make ARCH=x86_64 userspace
```

## Notes

- This is a stage-0 scaffold for future glibc porting.
- Dynamic linker, full libc ABI, and pthread/TLS support are still pending roadmap items.
