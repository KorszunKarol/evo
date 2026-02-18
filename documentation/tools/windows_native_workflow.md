# Windows Native Runtime Workflow

This project uses a Windows-first runtime path for interactive simulation and rendering.
WSL remains the preferred environment for coding agents and Linux tooling.

## Canonical Layout

- Repository location: `C:\dev\evolution`
- WSL access path: `/mnt/c/dev/evolution`
- Windows build directory: `build-win`
- WSL/Linux build directory: `build`

## One-Time Setup (Windows 11)

1. Install Visual Studio 2022 with C++ desktop tools.
2. Install CMake and Ninja (or ensure they are on `PATH`).
3. Install current NVIDIA graphics driver.
4. Open **Developer PowerShell for VS 2022** in `C:\dev\evolution`.

## Bootstrap + Configure

```powershell
.\tools\windows\bootstrap.ps1
.\tools\windows\configure.ps1 -BuildDir build-win -BuildType RelWithDebInfo
```

## Build + Test

```powershell
.\tools\windows\build.ps1 -BuildDir build-win
.\tools\windows\test.ps1 -BuildDir build-win
```

## Run Render Client

```powershell
.\tools\windows\run_render_client.ps1 -BuildDir build-win
```

## WSL Usage (Secondary)

- Use WSL for repository tooling, scripts, and analysis.
- Prefer running GUI/rendering natively on Windows for stable input and GPU performance.

## Local Validation Policy (No CI/CD)

Before merging to `master`, run local gates:

1. WSL gate: build + test (`build/`)
2. Windows gate: configure + build + test (`build-win/`)
3. Runtime gate: launch `render_client.exe` and verify controls + responsiveness
