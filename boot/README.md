# Boot Subsystem

Boot-time code and boot artifacts.

## Layout

- [`boot/grub/`](grub): GRUB config and Multiboot2 boot path
- [`boot/efi/`](efi): UEFI stub loader path (`BOOTX64.EFI`, OVMF flow)

## Build and run

- BIOS/GRUB ISO path:
  - `make ARCH=x86_64 image`
  - `make ARCH=x86_64 run`
- UEFI path:
  - `make ARCH=x86_64 uefi-stub`
  - `bash scripts/qemu/run-qemu-uefi.sh build/x86_64/boot/efi/x86_64/BOOTX64.EFI`
