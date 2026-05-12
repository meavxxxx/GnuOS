[CmdletBinding()]
param(
    [string]$Target = "full",
    [switch]$RebuildImage
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..\\..")).Path
$ImageName = "gnuos-dev"

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw "docker is not available. Install Docker Desktop first."
}

& docker image inspect $ImageName *> $null
$imageExists = ($LASTEXITCODE -eq 0)

if ($RebuildImage -or -not $imageExists) {
    & docker build -t $ImageName "$RepoRoot/scripts/docker"
    if ($LASTEXITCODE -ne 0) {
        throw "Docker image rebuild failed."
    }
}

switch ($Target.ToLowerInvariant()) {
    "full" { $makeArgs = "full" }
    "bootstrap" { $makeArgs = "full" }
    default { $makeArgs = $Target }
}

& docker run --rm -it -v "${RepoRoot}:/src" -w /src $ImageName bash -lc "make ARCH=x86_64 TARGET=x86_64-linux-gnu $makeArgs"
if ($LASTEXITCODE -ne 0) {
    throw "Docker make target failed: $Target"
}
