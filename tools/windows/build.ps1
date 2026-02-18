[CmdletBinding()]
param(
    [string]$BuildDir = 'build-win',
    [string[]]$Targets = @('sim_app', 'render_client', 'sim_tests'),
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\\..')).Path
$buildPath = Join-Path $repoRoot $BuildDir

Write-Host "== Build (Windows) =="
Write-Host "BuildDir: $buildPath"
Write-Host "Targets: $($Targets -join ', ')"
Write-Host "Configuration: $Configuration"

cmake --build $buildPath --config $Configuration --target $Targets
if ($LASTEXITCODE -ne 0) {
    throw "cmake build failed with exit code $LASTEXITCODE"
}
