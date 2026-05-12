# UEFI Boot Stub (x86_64)

This directory contains an early UEFI loader scaffold for GNU OS.

Current status:

- `boot/efi/x86_64/uefi_stub.c` builds into `BOOTX64.EFI`.
- Stub prints a boot banner through UEFI `ConOut`.
- Stub embeds `build/x86_64/kernel.elf`, maps `PT_LOAD` segments, resolves
  `kmain`, builds a multiboot2-compatible memory-map tag from UEFI
  `GetMemoryMap()` descriptors, and performs explicit `ExitBootServices`
  before kernel handoff.
- Stub probes UEFI GOP and, when available, emits a multiboot2 framebuffer
  tag so kernel early bring-up can consume framebuffer geometry/pixel layout.
- OVMF run helper is available in `scripts/qemu/run-qemu-uefi.sh`.

Build:

```bash
make ARCH=x86_64 uefi-stub
```

Run under QEMU + OVMF:

```bash
bash scripts/qemu/run-qemu-uefi.sh build/x86_64/boot/efi/x86_64/BOOTX64.EFI
```

Roadmap item `1.1` status: completed.
