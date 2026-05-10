[CmdletBinding()]
param(
    [ValidateSet("wsl", "docker", "both")]
    [string]$Backend = "both",
    [string]$Distro = "Ubuntu",
    [switch]$InstallCrossToolchain,
    [switch]$SkipBootstrapBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..\\..")).Path
$DockerImage = "gnuos-dev"

function Write-Step {
    param([string]$Message)
    Write-Host "[setup] $Message" -ForegroundColor Cyan
}

function Write-WarnLine {
    param([string]$Message)
    Write-Host "[setup] $Message" -ForegroundColor Yellow
}

function Assert-Command {
    param(
        [string]$Name,
        [string]$Hint
    )

    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Missing command '$Name'. $Hint"
    }
}

function Get-WslDistroList {
    $raw = & wsl.exe -l -q 2>$null
    if ($LASTEXITCODE -ne 0) {
        return @()
    }

    $normalized = (($raw | Out-String) -replace "`0", "")
    return ($normalized -split "`r?`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
}

function Ensure-WslDistro {
    param([string]$Name)

    Assert-Command -Name "wsl.exe" -Hint "Install WSL and reboot Windows, then run setup again."

    $distros = Get-WslDistroList
    if ($distros -contains $Name) {
        return $true
    }

    Write-WarnLine "WSL distro '$Name' not found. Running: wsl --install -d $Name"
    & wsl.exe --install -d $Name
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install WSL distro '$Name'. Install it manually and rerun setup."
    }

    Write-WarnLine "WSL install started. Finish installation/login in WSL, then rerun this script."
    return $false
}

function Convert-ToWslPath {
    param(
        [string]$WindowsPath,
        [string]$DistroName
    )

    $converted = & wsl.exe -d $DistroName -- wslpath -a -u "$WindowsPath"
    if ($LASTEXITCODE -ne 0) {
        throw "Could not convert Windows path to WSL path: $WindowsPath"
    }

    return ((($converted | Out-String) -replace "`0", "")).Trim()
}

function Invoke-WslBash {
    param(
        [string]$DistroName,
        [string]$Command
    )

    & wsl.exe -d $DistroName -- bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "WSL command failed: $Command"
    }
}

function Setup-Wsl {
    param(
        [string]$DistroName,
        [bool]$WithCrossToolchain,
        [bool]$RunBootstrapBuild
    )

    if (-not (Ensure-WslDistro -Name $DistroName)) {
        return
    }

    $repoLinuxPath = Convert-ToWslPath -WindowsPath $RepoRoot -DistroName $DistroName
    $bootstrapArgs = @()
    if ($WithCrossToolchain) {
        $bootstrapArgs += "--with-cross-toolchain"
    }

    $argLine = if ($bootstrapArgs.Count -gt 0) { " " + ($bootstrapArgs -join " ") } else { "" }

    Write-Step "Installing WSL build dependencies in distro '$DistroName'."
    Invoke-WslBash -DistroName $DistroName -Command "cd '$repoLinuxPath' && bash scripts/dev/wsl-bootstrap.sh$argLine"

    if ($RunBootstrapBuild) {
        $pathPrefix = if ($WithCrossToolchain) {
            "export PATH=`"`$HOME/.local/cross/x86_64-elf/bin:`$PATH`"; "
        } else {
            ""
        }

        Write-Step "Running bootstrap build in WSL (make kernel + make image)."
        Invoke-WslBash -DistroName $DistroName -Command "cd '$repoLinuxPath' && $pathPrefix make ARCH=x86_64 kernel && make ARCH=x86_64 image"
    }

    Write-Step "WSL setup complete."
}

function Setup-Docker {
    param([bool]$RunBootstrapBuild)

    Assert-Command -Name "docker" -Hint "Install Docker Desktop and make sure 'docker' is in PATH."

    Write-Step "Building Docker image '$DockerImage' from scripts/docker/Dockerfile."
    & docker build -t $DockerImage "$RepoRoot/scripts/docker"
    if ($LASTEXITCODE -ne 0) {
        throw "Docker image build failed."
    }

    if ($RunBootstrapBuild) {
        Write-Step "Running bootstrap build in Docker (make kernel + make image)."
        & docker run --rm -v "${RepoRoot}:/src" -w /src $DockerImage make ARCH=x86_64 TARGET=x86_64-linux-gnu kernel
        if ($LASTEXITCODE -ne 0) {
            throw "Docker kernel build failed."
        }

        & docker run --rm -v "${RepoRoot}:/src" -w /src $DockerImage make ARCH=x86_64 TARGET=x86_64-linux-gnu image
        if ($LASTEXITCODE -ne 0) {
            throw "Docker image build failed."
        }
    }

    Write-Step "Docker setup complete."
}

Write-Step "Repository root: $RepoRoot"
Write-Step "Backend mode: $Backend"

$doBootstrapBuild = -not $SkipBootstrapBuild
$hadErrors = $false

if ($Backend -eq "wsl" -or $Backend -eq "both") {
    try {
        Setup-Wsl -DistroName $Distro -WithCrossToolchain $InstallCrossToolchain.IsPresent -RunBootstrapBuild $doBootstrapBuild
    } catch {
        Write-WarnLine "WSL setup failed: $($_.Exception.Message)"
        $hadErrors = $true
    }
}

if ($Backend -eq "docker" -or $Backend -eq "both") {
    try {
        Setup-Docker -RunBootstrapBuild $doBootstrapBuild
    } catch {
        Write-WarnLine "Docker setup failed: $($_.Exception.Message)"
        $hadErrors = $true
    }
}

if ($hadErrors) {
    throw "One or more setup steps failed. See warnings above."
}

Write-Step "All requested setup steps completed."
Write-Host ""
Write-Host "Next commands:"
Write-Host "  WSL build:    .\\scripts\\dev\\wsl-make.ps1 kernel"
Write-Host "  WSL run:      .\\scripts\\dev\\wsl-make.ps1 run"
Write-Host "  Docker build: .\\scripts\\dev\\docker-make.ps1 kernel"
Write-Host "  Docker run:   .\\scripts\\dev\\docker-make.ps1 run"
