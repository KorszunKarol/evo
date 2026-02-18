[CmdletBinding()]
param(
    [string]$BuildDir = 'build-win',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$BuildType = 'RelWithDebInfo',
    [ValidateSet('auto', 'Ninja', 'Visual Studio 17 2022')]
    [string]$Generator = 'auto'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildPath = Join-Path $repoRoot $BuildDir

function Get-VsInstallPath {
    $pf = [Environment]::GetFolderPath('ProgramFilesX86')
    $vswhere = Join-Path $pf 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        return $null
    }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($path)) {
        return $null
    }
    return $path.Trim()
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string[]]$CommandArgs,
        [Parameter(Mandatory = $true)][string]$Description
    )

    & cmake @CommandArgs
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

$resolvedGenerator = $Generator
if ($Generator -eq 'auto') {
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        $resolvedGenerator = 'Ninja'
    } elseif (Get-VsInstallPath) {
        $resolvedGenerator = 'Visual Studio 17 2022'
    } else {
        throw "No supported generator detected. Install Ninja or Visual Studio 2022 C++ tools."
    }
}

Write-Host "== Configure (Windows) =="
Write-Host "Repo: $repoRoot"
Write-Host "BuildDir: $BuildDir"
Write-Host "BuildType: $BuildType"
Write-Host "Generator: $resolvedGenerator"

if ($resolvedGenerator -eq 'Ninja') {
    Invoke-Checked -Description 'cmake configure (Ninja)' -CommandArgs @(
        '-S', $repoRoot,
        '-B', $buildPath,
        '-G', 'Ninja',
        "-DCMAKE_BUILD_TYPE=$BuildType"
    )
} else {
    Invoke-Checked -Description 'cmake configure (Visual Studio)' -CommandArgs @(
        '-S', $repoRoot,
        '-B', $buildPath,
        '-G', 'Visual Studio 17 2022',
        '-A', 'x64'
    )
}
