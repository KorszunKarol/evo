[CmdletBinding()]
param(
    [string]$BuildDir = 'build-win'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\\..')
$buildPath = Join-Path $repoRoot $BuildDir
$exe = Join-Path $buildPath 'bin\\render_client.exe'

if (-not (Test-Path $exe)) {
    Write-Host "render_client.exe not found, building target first..."
    cmake --build $buildPath --target render_client
}

Write-Host "Launching: $exe"
& $exe
