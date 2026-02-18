# Module: Render Client

## Overview

The render client is an OpenGL simulation viewer that runs the live ECS simulation and exposes an observability-focused HUD. It renders terrain/water plus batched plant and creature points, supports selection/follow workflows, and provides runtime pacing controls (pause/realtime/step).

## Files

- `client/src/main.cpp` (thin bootstrap)
- `client/src/renderer/renderer_app.cpp`
- `client/src/renderer/camera_controller.cpp`
- `client/src/renderer/scene_extractor.cpp`

## Features

- GLFW + OpenGL 3.3 core rendering pipeline
- Live simulation execution in viewer loop (`SimulationApp` + scenario bootstrap)
- Terrain and water rendering
- Batched creature/plant rendering with distance-based size LOD
- Visibility counters (visible/culled) and draw-call diagnostics
- Click selection + inspector panel (species/diet/energy/position)
- Follow-selected camera mode
- Runtime pacing controls: pause, step, speed multiplier
- Overlay toggles (biome/creatures/plants)

## Dependencies

- `glfw` (windowing/input)
- `glad` (OpenGL loader)
- `glm` (math utilities)
- `imgui` (debug HUD)
- Links against the existing `sim_core` library

## Usage

Build target: `render_client`

Windows (recommended for interactive runtime):

```powershell
.\tools\windows\configure.ps1 -BuildDir build-win -BuildType RelWithDebInfo
.\tools\windows\build.ps1 -BuildDir build-win -Targets render_client
.\tools\windows\run_render_client.ps1 -BuildDir build-win
```

WSL/Linux:

```
cd build
cmake --build . --target render_client
./bin/render_client
```

WSL helper:

```bash
./tools/run_render_client_wsl.sh
```

Controls:

- **Orbit mode**: Arrows rotate, `WASD` pan target, `Q/E` zoom
- **Free mode**: Hold **RMB** to look, `WASD` move, `Q/E` move up/down
- **Space**: pause/resume simulation
- **N**: single simulation step
- **B**: toggle biome overlay
- **Mouse Left Click**: select nearest visible entity

## Future Enhancements

- GPU instancing path for creature/plant proxies
- Frustum culling and multi-band LOD meshes
- Sensor/combat overlays (vision rays, threat vectors, feeding links)
- Optional telemetry playback mode (replay snapshots without live ticking)
