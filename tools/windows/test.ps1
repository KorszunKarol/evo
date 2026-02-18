[CmdletBinding()]
param(
    [string]$BuildDir = 'build-win'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\\..')
$buildPath = Join-Path $repoRoot $BuildDir

Write-Host "== Test (Windows) =="
Write-Host "BuildDir: $buildPath"

ctest --test-dir $buildPath --output-on-failure
