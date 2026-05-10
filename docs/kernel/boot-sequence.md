# x86_64 Bootstrap Sequence

1. GRUB loads `kernel.elf` through Multiboot2.
2. `_start` (assembly) sets a bootstrap stack and calls `kmain`.
3. `kmain` initializes serial and prints startup diagnostics.
4. CPU enters idle loop with `hlt`.

This is intentionally minimal and acts as phase-0/1 scaffolding.

