[CmdletBinding()]
param(
    [string]$BuildDir = 'build-win',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$BuildType = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\\..')

Write-Host "== Configure (Windows) =="
Write-Host "Repo: $repoRoot"
Write-Host "BuildDir: $BuildDir"
Write-Host "BuildType: $BuildType"

cmake -S $repoRoot -B (Join-Path $repoRoot $BuildDir) -G Ninja -DCMAKE_BUILD_TYPE=$BuildType
