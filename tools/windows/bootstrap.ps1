[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$ScriptBlock,
        [Parameter(Mandatory = $true)][string]$Description
    )

    & $ScriptBlock
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE"
    }
}

function Test-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$Name
    )

    if (Get-Command $Name -ErrorAction SilentlyContinue) {
        Write-Host "[ok] $Name found"
        return $true
    }

    Write-Host "[missing] $Name not found"
    return $false
}

function Get-VsInstallPath {
    $pf = [Environment]::GetFolderPath('ProgramFilesX86')
    $vswhere = Join-Path $pf 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        return $null
    }

    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0) {
        return $null
    }
    if ([string]::IsNullOrWhiteSpace($path)) {
        return $null
    }
    return $path.Trim()
}

Write-Host "== Windows Toolchain Bootstrap Check =="
Write-Host "Date: $(Get-Date -Format o)"

$allOk = $true
$allOk = (Test-Tool -Name 'cmake') -and $allOk

$hasNinja = Test-Tool -Name 'ninja'
$hasCl = [bool](Get-Command cl -ErrorAction SilentlyContinue)
$vsInstall = Get-VsInstallPath

if ($hasCl) {
    Write-Host "[ok] cl found"
} else {
    Write-Host "[warn] cl not found in current shell"
}

if ($vsInstall) {
    Write-Host "[ok] Visual Studio C++ tools found at: $vsInstall"
} else {
    Write-Host "[warn] Visual Studio C++ tools not detected by vswhere"
}

if (-not $hasNinja -and -not $vsInstall) {
    $allOk = $false
}

if (Get-Command nvidia-smi -ErrorAction SilentlyContinue) {
    Write-Host "[ok] nvidia-smi found"
    nvidia-smi --query-gpu=name,driver_version --format=csv,noheader | Select-Object -First 1 | ForEach-Object {
        Write-Host "GPU: $_"
    }
} else {
    Write-Host "[warn] nvidia-smi not found (GPU check skipped)"
}

if (-not $allOk) {
    Write-Error "Required Windows toolchain is incomplete. Install either Ninja + cl toolchain, or Visual Studio 2022 with C++ build tools."
}

if ($hasNinja) {
    Write-Host "Preferred generator available: Ninja"
} elseif ($vsInstall) {
    Write-Host "Fallback generator available: Visual Studio 17 2022"
}

Write-Host "Bootstrap check passed."
