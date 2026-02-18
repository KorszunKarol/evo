[CmdletBinding()]
param(
    [string]$BuildDir = 'build-win',
    [string[]]$Targets = @('sim_app', 'render_client', 'sim_tests')
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\\..')
$buildPath = Join-Path $repoRoot $BuildDir

Write-Host "== Build (Windows) =="
Write-Host "BuildDir: $buildPath"
Write-Host "Targets: $($Targets -join ', ')"

cmake --build $buildPath --target $Targets
