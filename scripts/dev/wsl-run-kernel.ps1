[CmdletBinding()]
param(
    [string]$Distro = "Ubuntu",
    [string]$Arch = "x86_64",
    [switch]$SkipBuild,
    [switch]$Gui,
    [switch]$Headless,
    [int]$TimeoutSec = 0,
    [ValidateSet("wsl", "windows")]
    [string]$QemuHost = "wsl",
    [string]$WindowsQemuExe = "qemu-system-x86_64.exe",
    [string]$SerialLogPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..\\..")).Path
$buildTargets = "full"

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw "wsl.exe is not available. Install WSL first."
}

if ($RepoRoot.Length -lt 3 -or $RepoRoot[1] -ne ':') {
    throw "Failed to resolve WSL path for repo."
}
$drive = $RepoRoot.Substring(0, 1).ToLowerInvariant()
$rest = $RepoRoot.Substring(2).Replace('\', '/')
$repoLinuxPath = "/mnt/$drive$rest"
$repoLinuxPathSafe = $repoLinuxPath

if ($Gui -and $Headless) {
    throw "Use either -Gui or -Headless, not both."
}

function Invoke-WslBash {
    param(
        [string]$DistroName,
        [string]$Command
    )

    & wsl.exe -d $DistroName -- bash -lc $Command
}

function Test-WslGuiAvailable {
    param([string]$DistroName)

    & wsl.exe -d $DistroName -- bash -lc '[ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]'
    return ($LASTEXITCODE -eq 0)
}

function Test-WindowsQemuAvailable {
    param([string]$ExeName)

    $cmd = Get-Command $ExeName -ErrorAction SilentlyContinue
    return ($null -ne $cmd)
}

$kernelIsoPath = "build/$Arch/gnuos-$Arch.iso"
if (-not $SkipBuild) {
    $primaryBuildCmd = "source ~/.profile >/dev/null 2>&1 || true; cd '$repoLinuxPathSafe' && make ARCH=$Arch $buildTargets"
    Invoke-WslBash -DistroName $Distro -Command $primaryBuildCmd
    $primaryExitCode = $LASTEXITCODE
    if ($primaryExitCode -ne 0) {
        Write-Warning "Primary build failed (exit code: $primaryExitCode). Retrying full build in WSL tmpfs."
        $tmpBuildDir = "/tmp/gnuos-build/$Arch"
        $fallbackBuildCmd = "source ~/.profile >/dev/null 2>&1 || true; cd '$repoLinuxPathSafe' && rm -rf '$tmpBuildDir' && make ARCH=$Arch BUILD_DIR=$tmpBuildDir $buildTargets"
        Invoke-WslBash -DistroName $Distro -Command $fallbackBuildCmd
        $fallbackExitCode = $LASTEXITCODE
        if ($fallbackExitCode -ne 0) {
            throw "WSL kernel build failed (primary exit: $primaryExitCode, fallback exit: $fallbackExitCode)."
        }
        $kernelIsoPath = "$tmpBuildDir/gnuos-$Arch.iso"

        if ($QemuHost -eq "windows") {
            $copyIsoCmd = "source ~/.profile >/dev/null 2>&1 || true; cd '$repoLinuxPathSafe' && mkdir -p 'build/$Arch' && cp '$kernelIsoPath' 'build/$Arch/gnuos-$Arch.iso'"
            Invoke-WslBash -DistroName $Distro -Command $copyIsoCmd
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to copy fallback ISO from WSL tmpfs to Windows workspace."
            }
            $kernelIsoPath = "build/$Arch/gnuos-$Arch.iso"
        }
    }
}

$runGui = -not $Headless
if ($Gui) {
    $runGui = $true
}

if ($QemuHost -eq "wsl") {
    if ($runGui -and -not (Test-WslGuiAvailable -DistroName $Distro)) {
        throw @"
QEMU GUI window cannot be opened from WSL: DISPLAY/WAYLAND is not set.
Install/enable WSLg (or X server), then retry.
For now use: .\wsl-run-kernel.ps1 -Headless
"@
    }

    $qemuCmd = "qemu-system-x86_64 -cdrom '$kernelIsoPath' -serial stdio"
    if ($runGui) {
        $qemuCmd += " -display gtk"
    } else {
        $qemuCmd += " -display none"
    }
    if ($TimeoutSec -gt 0) {
        $qemuCmd = "timeout ${TimeoutSec}s $qemuCmd"
    }

    $runCmd = "source ~/.profile >/dev/null 2>&1 || true; cd '$repoLinuxPathSafe' && $qemuCmd"
    Invoke-WslBash -DistroName $Distro -Command $runCmd
    $exitCode = $LASTEXITCODE
    if ($TimeoutSec -gt 0 -and $exitCode -eq 124) {
        return
    }
    if ($exitCode -ne 0) {
        throw "WSL kernel run failed (exit code: $exitCode)."
    }
    return
}

if (-not (Test-WindowsQemuAvailable -ExeName $WindowsQemuExe)) {
    throw "Windows QEMU executable '$WindowsQemuExe' was not found in PATH. Install QEMU for Windows or pass -WindowsQemuExe."
}

$isoWindowsPath = Join-Path $RepoRoot "build\$Arch\gnuos-$Arch.iso"
if (-not (Test-Path -LiteralPath $isoWindowsPath)) {
    throw "ISO not found for Windows QEMU run: $isoWindowsPath"
}

if ([string]::IsNullOrWhiteSpace($SerialLogPath)) {
    $SerialLogPath = Join-Path $RepoRoot "build\$Arch\serial.log"
}
$serialLogDir = Split-Path -Parent $SerialLogPath
if (-not [string]::IsNullOrWhiteSpace($serialLogDir)) {
    New-Item -ItemType Directory -Force -Path $serialLogDir | Out-Null
}

if ($TimeoutSec -gt 0) {
    Write-Warning "TimeoutSec is ignored when -QemuHost windows is used."
}

$serialLogQemuPath = $SerialLogPath.Replace('\', '/')
$qemuArgs = @(
    "-cdrom", $isoWindowsPath,
    "-chardev", "stdio,id=serial0,signal=off,logfile=$serialLogQemuPath,logappend=on",
    "-serial", "chardev:serial0"
)

if ($runGui) {
    $qemuArgs += @("-display", "sdl")
} else {
    $qemuArgs += @("-display", "none")
}
Write-Host "QEMU host: Windows"
Write-Host "ISO: $isoWindowsPath"
Write-Host "Serial log file: $SerialLogPath"

& $WindowsQemuExe @qemuArgs
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "Windows QEMU run failed (exit code: $exitCode)."
}
