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

function Get-VsDevCmdPath {
    $installPath = Get-VsInstallPath
    if ([string]::IsNullOrWhiteSpace($installPath)) {
        return $null
    }
    $path = Join-Path $installPath 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path $path) {
        return $path
    }
    return $null
}

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

Write-Host "== Build (Windows) =="
Write-Host "BuildDir: $buildPath"
Write-Host "Targets: $($Targets -join ', ')"
Write-Host "Configuration: $Configuration"

$generator = Get-CMakeGenerator -BuildPath $buildPath
Write-Host "Generator: $generator"

if ($generator -eq 'NMake Makefiles') {
    $vsDevCmd = Get-VsDevCmdPath
    if ([string]::IsNullOrWhiteSpace($vsDevCmd)) {
        throw "VsDevCmd.bat not found. Install Visual Studio 2022 Build Tools."
    }
    $targetString = ($Targets -join ' ')
    $cmd = ('call "{0}" -arch=x64 && cmake --build "{1}" --target {2}' -f $vsDevCmd, $buildPath, $targetString)
    cmd.exe /d /c $cmd
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build (NMake + VsDevCmd) failed with exit code $LASTEXITCODE"
    }
} else {
    cmake --build $buildPath --config $Configuration --target $Targets
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build failed with exit code $LASTEXITCODE"
    }
}
