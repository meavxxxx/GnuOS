# x86_64 Bootstrap Sequence

1. GRUB loads `kernel.elf` through Multiboot2.
2. `_start` (assembly) sets a bootstrap stack and forwards Multiboot2 params to `kmain`.
3. `kmain` initializes serial output and loads a minimal IDT (vectors 0..31).
4. CPU exceptions route to `kpanic_exception()` with register dump over serial.
5. Multiboot memory map is parsed and used to seed a bootstrap PMM allocator.
6. Kernel memory range is reserved in PMM to avoid self-overwrite.
7. VMM bootstrap attaches to current PML4 (`CR3`) and supports 4K map/unmap.
8. Kernel virtual allocator maps pages from PMM in a dedicated VA region.
9. Large (2 MiB) bootstrap mappings can be split into 4K tables on demand.
10. Scheduler bootstrap initializes ready queue and creates idle task.
11. PIC is remapped, PIT timer is configured, and IRQ0/IRQ1 are unmasked.
12. Hardware interrupts are enabled and timer ticks reach scheduler.
13. Scheduler can dispatch kernel tasks using saved/restored task contexts.
14. Demo RCU reader/updater/reclaimer tasks exercise grace periods and callback reclaim.
15. CPU falls back to idle (`hlt`) only when no runnable task is available.

This is intentionally minimal and acts as phase-0/1 scaffolding.
