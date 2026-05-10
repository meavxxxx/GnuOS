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
11. PIC is remapped, PIT timer is configured, and IRQ0 is unmasked.
12. Hardware interrupts are enabled and timer ticks reach scheduler.
13. CPU enters idle loop with `hlt`.

This is intentionally minimal and acts as phase-0/1 scaffolding.
