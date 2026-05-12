[CmdletBinding()]
param(
    [string]$Distro = "Ubuntu",
    [string]$Arch = "x86_64",
    [switch]$SkipBuild,
    [switch]$Gui,
    [switch]$Headless,
    [int]$TimeoutSec = 0
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
    }
}

$qemuCmd = "qemu-system-x86_64 -cdrom '$kernelIsoPath' -serial stdio"
if ($Headless) {
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
