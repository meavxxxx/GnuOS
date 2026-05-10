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

# Docker
.\scripts\dev\docker-make.ps1 -RebuildImage kernel
.\scripts\dev\docker-make.ps1 run
```

