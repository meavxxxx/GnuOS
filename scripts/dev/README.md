# Local Dev Setup (Windows + WSL/Docker)

## One-command bootstrap

```powershell
.\scripts\dev\setup-dev.ps1 -Backend both
```

## Common options

- `-Backend wsl|docker|both`
- `-Distro Ubuntu` (default distro for WSL)
- `-InstallCrossToolchain` (builds `x86_64-elf-*` toolchain in WSL)
- `-SkipBootstrapBuild` (skip initial `make kernel` and `make image`)

## Daily usage

```powershell
# WSL
.\scripts\dev\wsl-make.ps1 kernel
.\scripts\dev\wsl-make.ps1 run
.\scripts\dev\wsl-run-kernel.ps1

# Docker
.\scripts\dev\docker-make.ps1 -RebuildImage kernel
.\scripts\dev\docker-make.ps1 run

# UEFI stub
.\scripts\dev\wsl-make.ps1 -Target uefi-stub
wsl bash scripts/qemu/run-qemu-uefi.sh build/x86_64/boot/efi/x86_64/BOOTX64.EFI
```

## GNU Mailman (roadmap 0.1)

```powershell
Copy-Item .\scripts\dev\mailman\env.example .\scripts\dev\mailman\.env
docker compose -f .\scripts\dev\mailman\docker-compose.yml --env-file .\scripts\dev\mailman\.env up -d
```

