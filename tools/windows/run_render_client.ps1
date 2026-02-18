[CmdletBinding()]
param(
    [string]$BuildDir = 'build-win',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\\..')).Path
$buildPath = Join-Path $repoRoot $BuildDir
$exe = Join-Path $buildPath 'bin\\render_client.exe'
$exeWithConfig = Join-Path $buildPath (Join-Path 'bin' (Join-Path $Configuration 'render_client.exe'))

if (Test-Path $exeWithConfig) {
    $exe = $exeWithConfig
}

if (-not (Test-Path $exe)) {
    Write-Host "render_client.exe not found, building target first..."
    cmake --build $buildPath --config $Configuration --target render_client
    if ($LASTEXITCODE -ne 0) {
        throw "render_client build failed with exit code $LASTEXITCODE"
    }
    if (Test-Path $exeWithConfig) {
        $exe = $exeWithConfig
    }
}

Write-Host "Launching: $exe"
& $exe
if ($LASTEXITCODE -ne 0) {
    throw "render_client exited with code $LASTEXITCODE"
}
