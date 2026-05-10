# Getting Started

## Prerequisites

- Cross compiler: `x86_64-elf-gcc`
- `grub-mkrescue`
- `xorriso`
- `qemu-system-x86_64`

## Build and run

```bash
make ARCH=x86_64 kernel
make ARCH=x86_64 image
make ARCH=x86_64 run
```

## Windows quick setup (PowerShell + WSL/Docker)

```powershell
# Run both WSL and Docker setup
.\scripts\dev\setup-dev.ps1 -Backend both

# If you want true cross toolchain in WSL:
.\scripts\dev\setup-dev.ps1 -Backend wsl -InstallCrossToolchain
```

```powershell
# Build or run inside WSL
.\scripts\dev\wsl-make.ps1 kernel
.\scripts\dev\wsl-make.ps1 image
.\scripts\dev\wsl-make.ps1 run
```

```powershell
# Build or run inside Docker
.\scripts\dev\docker-make.ps1 -RebuildImage kernel
.\scripts\dev\docker-make.ps1 image
.\scripts\dev\docker-make.ps1 run
```
