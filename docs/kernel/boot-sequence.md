# x86_64 Bootstrap Sequence

1. GRUB loads `kernel.elf` through Multiboot2.
2. `_start` (assembly) sets a bootstrap stack and calls `kmain`.
3. `kmain` initializes serial output and loads a minimal IDT (vectors 0..31).
4. CPU exceptions route to `kpanic_exception()` with register dump over serial.
5. A bootstrap physical memory allocator (PMM) is initialized.
6. CPU enters idle loop with `hlt`.

This is intentionally minimal and acts as phase-0/1 scaffolding.
