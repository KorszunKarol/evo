# Module: Render Client

## Overview

The render client is a lightweight OpenGL viewer that runs the headless simulation, collects collider data from the ECS, and renders each entity as analytic geometry. It provides an ImGui-powered HUD for quick diagnostics (body counts, contact totals, solver iterations) and an orbit camera for scene inspection.

## Files

- `client/src/main.cpp`

## Features

- GLFW + GLAD + OpenGL 3.3 core rendering pipeline
- Simple shader with per-instance model/color uniforms
- Analytic meshes for spheres, AABBs, and upright capsules (capsule rendered as cylinder + hemispheres)
- Deterministic stepping of the existing `SimulationApp` / `PhysicsSystem`
- ImGui overlay with live statistics and camera data
- Keyboard orbit controls (arrow keys rotate, W/S zoom)

## Dependencies

- `glfw` (windowing/input)
- `glad` (OpenGL loader)
- `glm` (math utilities)
- `imgui` (debug HUD)
- Links against the existing `sim_core` library

## Usage

Build target: `render_client`

```
cd build
cmake --build . --target render_client
./bin/render_client
```

Controls:

- **Arrow keys**: rotate orbit camera
- **W/S**: zoom in/out

## Future Enhancements

- Mouse-driven camera controls and UI toggles
- Render instancing for large populations
- Visualisation of contact normals and joint constraints
- Integration with sensor debugging overlays (vision rays, touch contacts)
