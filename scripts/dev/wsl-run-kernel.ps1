[CmdletBinding()]
param(
    [string]$Distro = "Ubuntu",
    [string]$Arch = "x86_64",
    [switch]$SkipBuild,
    [switch]$Gui,
    [int]$TimeoutSec = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..\\..")).Path

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw "wsl.exe is not available. Install WSL first."
}

if ($RepoRoot.Length -lt 3 -or $RepoRoot[1] -ne ':') {
    throw "Failed to resolve WSL path for repo."
}
$drive = $RepoRoot.Substring(0, 1).ToLowerInvariant()
$rest = $RepoRoot.Substring(2).Replace('\', '/')
$repoLinuxPath = "/mnt/$drive$rest"

# Path from wslpath is expected without single quotes in this workspace.
$repoLinuxPathSafe = $repoLinuxPath
$kernelIsoPath = "build/$Arch/gnuos-$Arch.iso"

$setupCmd = "source ~/.profile >/dev/null 2>&1 || true"
$cdCmd = "cd '$repoLinuxPathSafe'"
$buildCmd = "make ARCH=$Arch image"

$qemuCmd = "qemu-system-x86_64 -cdrom '$kernelIsoPath' -serial stdio"
if (-not $Gui) {
    $qemuCmd += " -display none"
}
if ($TimeoutSec -gt 0) {
    $qemuCmd = "timeout ${TimeoutSec}s $qemuCmd"
}

$commands = @($setupCmd, $cdCmd)
if (-not $SkipBuild) {
    $commands += $buildCmd
}
$commands += $qemuCmd

$cmd = [string]::Join(" && ", $commands)
& wsl.exe -d $Distro -- bash -lc $cmd
$exitCode = $LASTEXITCODE
if ($TimeoutSec -gt 0 -and $exitCode -eq 124) {
    return
}
if ($exitCode -ne 0) {
    throw "WSL kernel run failed (exit code: $exitCode)."
}
