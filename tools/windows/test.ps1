[CmdletBinding()]
param(
    [string]$BuildDir = 'build-win',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\\..')).Path
$buildPath = Join-Path $repoRoot $BuildDir

Write-Host "== Test (Windows) =="
Write-Host "BuildDir: $buildPath"
Write-Host "Configuration: $Configuration"

ctest --test-dir $buildPath -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "ctest failed with exit code $LASTEXITCODE"
}
