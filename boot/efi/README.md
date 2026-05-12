# UEFI Boot Stub (x86_64)

This directory contains an early UEFI loader scaffold for GNU OS.

Current status:

- `boot/efi/x86_64/uefi_stub.c` builds into `BOOTX64.EFI`.
- Stub prints a boot banner through UEFI `ConOut`.
- Stub embeds `build/x86_64/kernel.elf`, maps `PT_LOAD` segments, resolves
  `kmain`, and includes a prototype 64-bit handoff path with multiboot2 magic
  + synthetic mmap boot info block.
- OVMF run helper is available in `scripts/qemu/run-qemu-uefi.sh`.

Build:

```bash
make ARCH=x86_64 uefi-stub
```

Run under QEMU + OVMF:

```bash
bash scripts/qemu/run-qemu-uefi.sh build/x86_64/boot/efi/x86_64/BOOTX64.EFI
```

Next milestone for roadmap item `1.1`:

- construct multiboot-compatible memory-map payload instead of fallback pointer,
- add explicit `ExitBootServices` transition before kernel handoff.
