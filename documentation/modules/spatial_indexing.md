# Module: Spatial Indexing

## Overview

Spatial indexing provides efficient spatial queries (radius search, nearest-neighbor) for the simulation. It's essential for performance in systems that need to find entities by location (vision, feeding, social behavior, decomposition).

## Files

- `/home/karolito/evolution/sim/include/evolution/sim/creature_spatial_index.h`
- `/home/karolito/evolution/sim/src/systems/creature_spatial_index_system.cpp`
- `/home/karolito/evolution/sim/include/evolution/sim/environment/environment.h` (PlantSpatialIndex definition)
- `/home/karolito/evolution/sim/src/environment/environment.cpp` (PlantSpatialIndex implementation)

## Index Types

### CreatureSpatialIndex

**Purpose**: Accelerates queries for living creatures and corpses.

**Data Structure**:
```cpp
class CreatureSpatialIndex {
    using CellKey = std::pair<int32_t, int32_t>; // (ix, iz)
    using Cell = std::vector<entt::entity>;

    std::unordered_map<CellKey, Cell> cells_;
    double cell_size_; // World-space cell size (e.g., 2.0m)
    int cells_x_, cells_z_; // Grid dimensions
};
```

**Operations**:
- `clear()` - Empty all cells
- `rebuild(registry)` - Re-index all creatures with TransformComponent
- `insert(entity, position)` - Add/update entity in appropriate cell
- `for_each_in_radius(registry, position, radius, callback)` - Execute callback on all entities within radius

**Complexity**:
- Insert: O(1) amortized (hash table)
- Radius query: O(C + R²) where C = cells in radius, R = average entities per cell

### PlantSpatialIndex

**Purpose**: Accelerates queries for plants (herbivore foraging, seeding distance checks).

**Data Structure**:
```cpp
class PlantSpatialIndex {
    using CellKey = std::pair<int32_t, int32_t>;
    using Cell = std::vector<entt::entity>;

    std::unordered_map<CellKey, Cell> cells_;
    double cell_size_;
    int cells_x_, cells_z_;
};
```

**Operations**: Same interface as CreatureSpatialIndex
- `for_each_in_radius(registry, position, radius, callback)` - Find plants within feeding radius

## Integration

### Systems Using Spatial Indices

| System | Index Used | Purpose |
|--------|-------------|---------|
| **VisionSystem** | CreatureSpatialIndex | Cull raycast candidates to nearby entities only |
| **FeedingSystem** | CreatureSpatialIndex, PlantSpatialIndex | Find nearby prey (predators) or plants (herbivores) |
| **SocialBehaviorSystem** | CreatureSpatialIndex | Find pack members, intruders within social radius |
| **PlantSeedingSystem** | PlantSpatialIndex | Check crowding before spawning new plants |
| **DecompositionSystem** | CreatureSpatialIndex | Find corpses to decompose (if querying by region) |

### Index Update Strategy

**Rebuild Strategy**:
- **PlantSpatialIndex**: Rebuilt every tick by PlantSpatialSystem
- **CreatureSpatialIndex**: Rebuilt every tick (integrated into index update loop)

**Update Frequency Trade-off**:
- **Every tick**: More accurate, higher CPU cost
- **Every N ticks**: Faster, but stale position data

## Performance Characteristics

### Cell Size Selection

**Small Cell Size** (e.g., 1.0m):
- **Pros**: Fewer entities per cell, faster radius queries
- **Cons**: More cells, higher memory, slower rebuild

**Large Cell Size** (e.g., 10.0m):
- **Pros**: Fewer cells, lower memory, faster rebuild
- **Cons**: More entities per cell, slower radius queries

**Optimal Cell Size**:
- Choose based on average entity spacing
- Target 5-15 entities per cell (empirical sweet spot)

### Radius Query Optimization

```cpp
void for_each_in_radius(Vec3 center, double radius, Callback callback) {
    // Compute cell bounds covering radius
    int min_ix = floor((center.x - radius) / cell_size_);
    int max_ix = ceil((center.x + radius) / cell_size_);
    int min_iz = floor((center.z - radius) / cell_size_);
    int max_iz = ceil((center.z + radius) / cell_size_);

    // Iterate all cells in bounding box
    for (int ix = min_ix; ix <= max_ix; ix++) {
        for (int iz = min_iz; iz <= max_iz; iz++) {
            auto it = cells_.find({ix, iz});
            if (it != cells_.end()) {
                for (auto entity : it->second) {
                    // Distance check against world position
                    auto& transform = registry.get<TransformComponent>(entity);
                    double dist = (transform.position - center).length();
                    if (dist <= radius) {
                        callback(entity, transform.position);
                    }
                }
            }
        }
    }
}
```

## Memory Footprint

### CreatureSpatialIndex

**Per-Entity Overhead**: O(1) (hash table entry)
- **Total Memory**: O(C + E×B) where C = cells, E = average entities per cell, B = bucket size (unordered_map constant)

### PlantSpatialIndex

Similar footprint to CreatureSpatialIndex

**Combined Memory**: ~O(2C + 2E×B) for both indices

## Configuration

**Grid Dimensions** (EnvironmentConfig):
- `cell_size` (double) - World-space cell edge length (default: 2.0m)
- `world_size_x` (double) - Total world width (from terrain config)
- `world_size_z` (double) - Total world depth (from terrain config)

**Derived Grid Size**:
```cpp
cells_x_ = ceil(world_size_x / cell_size);
cells_z_ = ceil(world_size_z / cell_size);
```

## Debugging and Telemetry

**Metrics to Track**:
- Average cell occupancy (entities/cell)
- Query latency distribution (p50, p95, p99)
- Rebuild time (ms)
- Cache miss rate (if caching implemented)

**Visualization** (Render Client):
- Draw cell grid overlays
- Color-code by occupancy (green = sparse, yellow = medium, red = dense)
- Highlight radius queries during execution

## Future Optimizations

### Chunked Spatial Hash

**Approach**: Divide world into chunks (e.g., 64×64 cells), each chunk maintains its own hash table.

**Benefits**:
- Better cache locality (chunks accessed sequentially)
- Parallelizable chunk updates
- Reduced contention on global hash table

### Dynamic Cell Size

**Approach**: Adapt cell size based on local density (fine cells in dense areas, coarse in sparse).

**Benefits**:
- Balanced query performance across variable-density regions
- Reduced memory in sparse areas

## Related Documentation

- [Environment Module](./environment.md) - PlantSpatialIndex usage
- [Social Behavior Module](./social_behavior.md) - CreatureSpatialIndex for pack/flocking queries
- [Feeding Module](./environment.md#feeding-system) - Radius queries for foraging
- [Vision Module](./vision.md) - Culling candidates via spatial index
