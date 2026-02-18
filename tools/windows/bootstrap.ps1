[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

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

Write-Host "== Windows Toolchain Bootstrap Check =="
Write-Host "Date: $(Get-Date -Format o)"

$allOk = $true
$allOk = (Test-Tool -Name 'cmake') -and $allOk
$allOk = (Test-Tool -Name 'ninja') -and $allOk
$allOk = (Test-Tool -Name 'cl') -and $allOk

if (Get-Command nvidia-smi -ErrorAction SilentlyContinue) {
    Write-Host "[ok] nvidia-smi found"
    nvidia-smi --query-gpu=name,driver_version --format=csv,noheader | Select-Object -First 1 | ForEach-Object {
        Write-Host "GPU: $_"
    }
} else {
    Write-Host "[warn] nvidia-smi not found (GPU check skipped)"
}

if (-not $allOk) {
    Write-Error "Required tools missing. Open Developer PowerShell for Visual Studio and ensure CMake + Ninja are installed."
}

Write-Host "Bootstrap check passed."
