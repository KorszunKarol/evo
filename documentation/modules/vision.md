# Module: Vision System

## Overview

The Vision System provides sensory perception for creatures using raycasting. It detects objects in the environment (plants, prey, predators, terrain) and generates inputs for neural brains.

## Files

- `/home/karolito/evolution/sim/include/evolution/sim/vision_system.h`
- `/home/karolito/evolution/sim/src/systems/vision_system.cpp`

## Component Definitions

### VisionComponent

**Purpose**: Configures sensor parameters and stores raycast results for a creature.

**Fields**:
- `fov_radians` (double) - Field of view angle in radians (typical range: π/2 to π)
- `ray_count` (uint32_t) - Number of rays to cast (sensor resolution)
- `max_range` (double) - Maximum detection distance in meters
- `enabled` (bool) - Whether vision is active (can be disabled for underground/blind creatures)
- `ray_distances` (std::vector<double>) - Per-ray distance to first hit
- `ray_hit_types` (std::vector<VisionHitType>) - Per-ray hit classification
- `ray_hit_entities` (std::vector<entt::entity>) - Per-ray hit entity references

**Data Contract**:
- **Readers**: BrainInferenceSystem, TelemetrySystem
- **Writers**: VisionSystem
- **Lifetime**: Updated every tick (or at configured interval)

### VisionHitType (Enum)

**Purpose**: Classifies what a vision ray hit.

**Values**:
- `Plant` - Hit a plant entity
- `Terrain` - Hit terrain/ground
- `Corpse` - Hit a dead body (CorpseComponent)
- `Agent` - Hit another living creature
- `Unknown` - No valid hit (out of range or missing data)

## VisionSystem

**Purpose**: Casts rays from creature position, detects objects within environment, populates VisionComponent with sensory data.

**Algorithm**:
1. For each entity with VisionComponent:
   - Get TransformComponent (world position, orientation)
   - Read VisionComponent configuration (fov, ray_count, max_range)
2. For each ray in ray_count:
   - Calculate ray direction (spread evenly across FOV)
   - Cast ray via IPhysicsBackend::raycast(origin, direction, max_range)
   - Record closest hit (if any)
3. Populate VisionComponent.ray_distances, ray_hit_types, ray_hit_entities

**Complexity**: O(R × E) where R = ray_count, E = number of entities with colliders

**Optimizations**:
- Spatial indexing (CreatureSpatialIndex, PlantSpatialIndex) reduces raycast candidates
- Early termination on terrain hit (prevents穿透检测)
- Batched raycasting (if physics backend supports)

**Integration with Physics Backend**:
```cpp
auto result = physics_backend->raycast(
    creature_transform.position,
    ray_direction,
    vision.max_range
);

if (result.has_value()) {
    vision.ray_distances[i] = result->distance;
    vision.ray_hit_types[i] = result->type;
    vision.ray_hit_entities[i] = result->entity;
}
```

## Sensor Integration

### Vision as Brain Input

Vision data is converted to brain inputs in BrainInferenceSystem:

**Simplified Encoding** (low-dimensional inputs):
- Ray count = 8 (4 cardinal + 4 diagonal)
- Input mapping: `[left_dist, left_diag, forward_dist, right_diag, right_dist, behind_left, behind, behind_right, behind_diag]`
- Normalized distances: `clamp(distance / max_range, 0.0, 1.0)`

**High-Dimisional Encoding** (for NEAT brains):
- All ray distances as separate inputs
- All ray hit types as one-hot encoded inputs
- Entity-specific data (e.g., prey size, species) encoded via BrainInspectComponent

### Social Signals from Vision

Vision data contributes to SocialSignalsComponent computation:

- `prey_dir` (Vec3) - Normalized direction to nearest detected prey
- `pack_density` (uint32_t) - Count of visible pack members
- `intruder_density` (uint32_t) - Count of visible intruders
- `intruder_density_near_prey` (uint32_t) - Intruders near detected prey

## Performance Considerations

**Optimizations**:
- **Spatial Indexing**: Only raycast against entities in nearby spatial hash cells
- **Culling**: Skip entities behind creature (dot product with forward vector < 0)
- **Interval-based Updates**: Update vision every N ticks instead of every tick (configurable per genome)
- **Ray Consolidation**: Group nearby entities into single "blob" hit for efficiency

**Memory Footprint**:
- Per creature: O(ray_count) for distances + O(ray_count) for hit types + O(ray_count) for entities
- Total: O(C × R) where C = creature count, R = ray_count

## Configuration Examples

**Herbivore Vision**:
```cpp
VisionComponent vision;
vision.fov_radians = 1.5708; // ~90 degrees
vision.ray_count = 12;
vision.max_range = 15.0; // meters
vision.enabled = true;
```

**Predator Vision**:
```cpp
VisionComponent vision;
vision.fov_radians = 2.356; // ~135 degrees (wide FOV for hunting)
vision.ray_count = 16; // Higher resolution
vision.max_range = 30.0; // Longer range
vision.enabled = true;
```

**Underground/Cave Creature**:
```cpp
VisionComponent vision;
vision.fov_radians = 0.785; // ~45 degrees
vision.ray_count = 4;
vision.max_range = 5.0;
vision.enabled = true; // Could be false for blind creatures
```

## Debugging and Telemetry

**BrainInspectComponent Integration**:
- Stores ray cast snapshots for debugging
- Visualizes vision rays in client render
- Validates sensor coverage and hit detection

**Telemetry Metrics**:
- Average ray hit distance (environment density)
- Hit type distribution (plant vs terrain vs agent ratio)
- Vision system execution time (performance profiling)

## Related Documentation

- [Components Module](./components.md) - VisionComponent, VisionHitType definitions
- [Brain Module](./brain.md) - Brain input encoding from vision data
- [Physics Module](./physics_system.md) - PhysicsBackend::raycast interface
- [Client Module](./render_client.md) - Vision ray visualization in render client
