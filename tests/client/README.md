# Render Client Test Suite

Comprehensive test suite for the evolution simulation render client (`client/src/main.cpp`).

## Overview

This test suite provides extensive coverage of the render client's functionality, including:

- **Shader compilation and linking** - Validates GLSL shader compilation, error handling, and program linking
- **Mesh creation** - Tests terrain, water, sphere, and cylinder mesh generation
- **Instance buffer management** - Verifies dynamic buffer allocation and instance data upload
- **ECS data extraction** - Tests reading from the ECS registry for rendering
- **Camera calculations** - Validates orbit camera position computations
- **Color utilities** - Tests energy-to-color and species color mappings
- **OpenGL context management** - Headless OpenGL context for CI/testing

## Test Structure

### Test Fixtures (`test_render_fixtures.h/cpp`)

- **`OpenGLTestContext`** - Manages a headless OpenGL 3.3 core context using GLFW
- **`RenderClientFixture`** - Base fixture providing OpenGL context and simulation setup
- Helper functions for creating test terrain, biome maps, and water maps

### Test Files

1. **`test_shader.cpp`** - Shader compilation and linking tests
   - Valid shader compilation
   - Invalid shader error handling
   - Terrain, water, and instanced shader validation

2. **`test_mesh.cpp`** - Mesh creation and validation
   - Water mesh creation
   - Terrain mesh data validation
   - Mesh cleanup

3. **`test_color_utils.cpp`** - Color calculation utilities
   - Energy-to-color interpolation (low/mid/high)
   - Plant species color mapping
   - Color clamping and wrapping

4. **`test_camera.cpp`** - Camera calculations
   - Orbit camera position computation
   - Yaw/pitch rotation
   - Distance and target offset

5. **`test_ecs_extraction.cpp`** - ECS data extraction
   - Terrain/biome/water map availability
   - Agent component validation
   - Plant component validation
   - Transform position validation
   - Energy ratio calculations

6. **`test_instance_buffer.cpp`** - Instance buffer management
   - Buffer creation and cleanup
   - Capacity expansion
   - Instance data upload
   - Multiple upload scenarios

## Building and Running

### Prerequisites

- OpenGL 3.3+ support (or Mesa/software rendering for headless)
- GLFW 3.3+
- GLAD
- GoogleTest

### Build

```bash
cd build
cmake ..
cmake --build . --target render_client_tests
```

### Run

```bash
./bin/render_client_tests
```

Or with CTest:

```bash
ctest -R render_client
```

## CI/CD Considerations

The test suite is designed to gracefully handle environments without OpenGL support:

- Tests use `GTEST_SKIP()` if OpenGL context creation fails
- Headless context creation uses hidden GLFW windows
- Tests will automatically skip in environments without display servers

For CI environments without GPU access, consider:

1. **Mesa/OSMesa** - Software OpenGL implementation
2. **Xvfb** - Virtual framebuffer for headless X11
3. **Docker with GPU passthrough** - For GPU-accelerated CI

## Test Coverage

### Shader Tests
- ✅ Valid shader compilation
- ✅ Invalid shader error handling
- ✅ Program linking
- ✅ All three shader types (terrain, water, instanced)

### Mesh Tests
- ✅ Water mesh creation
- ✅ Terrain mesh data validation
- ✅ Mesh cleanup

### Color Utilities
- ✅ Energy-to-color mapping (0.0, 0.5, 1.0)
- ✅ Interpolation validation
- ✅ Plant species color palette
- ✅ Color wrapping for large species IDs

### Camera Tests
- ✅ Default position calculation
- ✅ Yaw rotation
- ✅ Pitch rotation
- ✅ Distance validation
- ✅ Target offset
- ✅ Combined rotations

### ECS Extraction
- ✅ Terrain availability
- ✅ Biome map sampling
- ✅ Water map access
- ✅ Agent component validation
- ✅ Plant component validation
- ✅ Transform positions
- ✅ Collider scales
- ✅ Energy ratios

### Instance Buffers
- ✅ Buffer creation
- ✅ Capacity expansion
- ✅ Data upload
- ✅ Empty upload handling
- ✅ Multiple uploads
- ✅ Cleanup

## Future Enhancements

Potential additions to the test suite:

1. **Transform History Interpolation** - Test entity position interpolation between frames
2. **Render Toggles** - Test UI toggle state management
3. **Time Controls** - Test pause/step/time scale functionality
4. **Integration Tests** - End-to-end rendering pipeline tests
5. **Performance Tests** - Benchmark mesh creation and instance buffer uploads
6. **Visual Regression Tests** - Screenshot comparison for rendering output

## Notes

- Many functions in `main.cpp` are in an anonymous namespace, so tests recreate the logic for validation
- For full integration testing, consider extracting render functions to a separate library
- OpenGL state is checked after each test to catch errors early
- Tests use deterministic seeds for reproducible results

## References

- [GoogleTest Documentation](https://google.github.io/googletest/)
- [OpenGL Testing Best Practices](https://www.khronos.org/opengl/wiki/OpenGL_Context)
- [GLFW Documentation](https://www.glfw.org/docs/latest/)

