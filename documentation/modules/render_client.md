# Module: Render Client

## Overview

The render client is a lightweight OpenGL viewer that runs the headless simulation, collects collider data from the ECS, and renders each entity as analytic geometry. It provides an ImGui-powered HUD for quick diagnostics (body counts, contact totals, solver iterations) and an orbit camera for scene inspection.

## Files

- `client/src/main.cpp`
- `client/src/terrain_textures.cpp`
- `client/include/evolution/client/terrain_textures.h`

## Features

- GLFW + GLAD + OpenGL 3.3 core rendering pipeline
- Advanced terrain shading with:
  - **Triplanar mapping** for steep slopes (cliffs) to eliminate texture stretching
  - **Normal maps** for enhanced surface detail and lighting
  - **Macro/micro variation** using procedural noise to break tiling patterns
  - **Stochastic texture sampling** (optional) for highly repetitive materials
  - Height/slope/biome/water-based material blending
- Procedural texture generation for terrain materials (grass, soil, rock, sand, snow)
- Simple shader with per-instance model/color uniforms
- Analytic meshes for spheres, AABBs, and upright capsules (capsule rendered as cylinder + hemispheres)
- Deterministic stepping of the existing `SimulationApp` / `PhysicsSystem`
- ImGui overlay with live statistics, camera data, and terrain shading controls
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

## Terrain Shading System

The render client implements a comprehensive terrain shading pipeline supporting multiple advanced techniques:

### TerrainTextures

**Purpose**: Manages procedural texture generation and texture array management for terrain materials.

**Public API**:
```cpp
explicit TerrainTextures(int resolution = 512);
void bind_albedo_array(GLuint unit) const noexcept;
void bind_normal_array(GLuint unit) const noexcept;
```

**Materials**: Supports 5 material types (grass, soil, rock, sand, snow) stored in texture arrays.

**Thread Safety**: Not thread-safe; must be used from the OpenGL context thread.

### Shader Features

**Phase 1.5 - Triplanar Mapping**:
- Automatically activates on steep slopes (configurable threshold)
- Samples textures from XY, XZ, and YZ projections
- Blends based on surface normal components
- Eliminates texture stretching on cliffs

**Phase 2 - Normal Maps and Macro/Micro Variation**:
- Per-material normal maps for enhanced surface detail
- Macro variation using deterministic noise based on world position and global seed
- Micro detail from high-frequency normal maps
- Preserves determinism across runs

**Phase 3 - Stochastic Sampling** (Optional):
- Deterministic hash-based texture coordinate offsets
- Reduces visible tiling patterns
- Configurable intensity
- Only applied to repetitive ground materials (e.g., grass)

### Controls

All terrain shading features can be toggled via ImGui:
- **Triplanar Mapping**: Enable/disable with cliff threshold slider
- **Normal Maps**: Enable/disable normal map sampling
- **Stochastic Sampling**: Enable/disable with intensity slider

### Performance

- Texture arrays use mipmaps for efficient sampling
- Triplanar mapping only activates on steep slopes (minority of pixels)
- Stochastic sampling limited to specific materials
- All features maintain deterministic rendering per seed

## Future Enhancements

- Mouse-driven camera controls and UI toggles
- Render instancing for large populations
- Visualisation of contact normals and joint constraints
- Integration with sensor debugging overlays (vision rays, touch contacts)
- BiomeMap and WaterMap integration for more accurate material selection
