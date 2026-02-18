[CmdletBinding()]
param(
    [string]$BuildDir = 'build-win',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\\..')).Path
$buildPath = Join-Path $repoRoot $BuildDir

function Get-CMakeGenerator {
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $cachePath = Join-Path $BuildPath 'CMakeCache.txt'
    if (-not (Test-Path $cachePath)) {
        throw "Missing CMakeCache.txt in $BuildPath. Run configure first."
    }
    $line = Get-Content $cachePath | Where-Object { $_ -like 'CMAKE_GENERATOR:*' } | Select-Object -First 1
    if (-not $line) {
        throw "Unable to determine CMake generator from $cachePath."
    }
    return ($line -split '=', 2)[1]
}

Write-Host "== Test (Windows) =="
Write-Host "BuildDir: $buildPath"
Write-Host "Configuration: $Configuration"

$generator = Get-CMakeGenerator -BuildPath $buildPath
Write-Host "Generator: $generator"

if ($generator -eq 'NMake Makefiles') {
    ctest --test-dir $buildPath --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "ctest failed with exit code $LASTEXITCODE"
    }
} else {
    ctest --test-dir $buildPath -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "ctest failed with exit code $LASTEXITCODE"
    }
}
