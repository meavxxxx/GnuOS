[CmdletBinding()]
param(
    [string]$Target = "kernel",
    [string]$Distro = "Ubuntu"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..\\..")).Path

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw "wsl.exe is not available. Install WSL first."
}

$repoLinuxPath = (((& wsl.exe -d $Distro -- wslpath -a -u "$RepoRoot" | Out-String) -replace "`0", "")).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Failed to resolve WSL path for repo."
}

$cmd = "source ~/.profile >/dev/null 2>&1 || true; cd '$repoLinuxPath' && make ARCH=x86_64 $Target"
& wsl.exe -d $Distro -- bash -lc $cmd
if ($LASTEXITCODE -ne 0) {
    throw "WSL make target failed: $Target"
}
