[CmdletBinding()]
param(
    [string]$Target = "full",
    [string]$Distro = "Ubuntu",
    [string]$Arch = "x86_64"
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

$makeArgs = switch ($Target.ToLowerInvariant()) {
    "full" { "full" }
    "bootstrap" { "full" }
    default { $Target }
}

$cmd = "source ~/.profile >/dev/null 2>&1 || true; cd '$repoLinuxPath' && make ARCH=$Arch $makeArgs"
& wsl.exe -d $Distro -- bash -lc $cmd
if ($LASTEXITCODE -ne 0) {
    throw "WSL make target failed: $Target (ARCH=$Arch)"
}
