# Module: Environment Systems

## Overview

The environment module provides terrain generation, soil nutrient simulation, plant lifecycle management, and feeding interactions. These systems create a living world where creatures can graze, plants grow and reproduce, and resources cycle through the ecosystem.

## Files

- `sim/include/evolution/sim/environment/environment.h`
- `sim/src/environment/environment.cpp`
- `sim/include/evolution/sim/environment/environment_bootstrap.h`
- `sim/src/environment/environment_bootstrap.cpp`
- `sim/include/evolution/sim/environment/soil_system.h`
- `sim/src/environment/soil_system.cpp`
- `sim/include/evolution/sim/environment/soil_volume.h`
- `sim/src/environment/soil_volume.cpp`
- `sim/include/evolution/sim/environment/plant_systems.h`
- `sim/src/environment/plant_systems.cpp`
- `sim/include/evolution/sim/environment/feeding_system.h`
- `sim/src/environment/feeding_system.cpp`

## Key Types

### Terrain

**Purpose**: Deterministic heightfield representing the simulation world surface.

**Public API**:
```cpp
explicit Terrain(const TerrainConfig& config);
double height(double x, double z) const noexcept;
Vec3 normal(double x, double z) const noexcept;
double min_y() const noexcept;
double max_y() const noexcept;
```

**Parameters**:
- `TerrainConfig`:
  - `width_cells`, `height_cells`: Grid resolution (default: 512×512)
  - `cell_size`: World-space distance between samples (default: 1.0m)
  - `elevation_scale`: Maximum height amplitude (default: 20.0m)
  - `seed`: Deterministic noise seed (default: 1337)
  - `octaves`, `base_frequency`, `lacunarity`, `gain`: Fractal noise parameters

**Returns**:
- `height()`: Terrain elevation at (x, z) in meters
- `normal()`: Unit-length surface normal vector
- `min_y()` / `max_y()`: Bounding height range

**Implementation Details**:
- Uses Perlin-style noise with multiple octaves
- Bilinear interpolation for continuous queries
- Central differences for normal computation
- Clamps queries outside domain to edge values

**Thread Safety**: Read-only queries are thread-safe; construction is not thread-safe.

**Storage**: Stored in `registry.ctx<Terrain>()` as a global service.

### SoilGrid

**Purpose**: 2D nutrient field consumed by plants and regenerated over time.

**Public API**:
```cpp
explicit SoilGrid(const SoilConfig& config);
float& at(int ix, int iz) noexcept;
float sample(double x, double z) const noexcept;
void diffuse(double dt) noexcept;
void regenerate(double dt) noexcept;
double mean_nutrient() const noexcept;
```

**Parameters**:
- `SoilConfig`:
  - `width_cells`, `height_cells`: Grid resolution (default: 256×256)
  - `cell_size`: World-space cell size (default: 2.0m)
  - `max_nutrient`: Upper bound per cell (default: 10.0)
  - `diffusion_rate`: Diffusion coefficient (default: 0.5)
  - `regeneration_rate`: Baseline regen per second (default: 0.05)
  - `baseline_nutrient`: Floor value maintained (default: 4.0)

**Returns**:
- `at()`: Direct cell access (mutable)
- `sample()`: Bilinearly interpolated value at world position
- `mean_nutrient()`: Arithmetic mean across all cells

**Implementation Details**:
- Five-point stencil diffusion with reflective boundaries
- Separate scratch buffer to avoid allocations during diffusion
- Clamps values to [0, max_nutrient] after each step

**Thread Safety**: Not thread-safe; mutations occur during `SoilSystem::tick()`.

**Storage**: Stored in `registry.ctx<SoilGrid>()` as a global service.

### SoilVolume

**Purpose**: 3D voxel-based soil field used for volumetric nutrient sampling.

**Public API**:
```cpp
explicit SoilVolume(const SoilVolumeConfig& config);
SoilVoxel& at(int x, int y, int z);
const SoilVoxel& at(int x, int y, int z) const;
SoilVoxel sample(const Vec3& pos) const;
void diffuse(double dt);
void regenerate(double dt, const BiomeMap* biome_map, double climate_mult);
int width() const;
int height() const;
int depth() const;
double voxel_size() const;
```

**Parameters**:
- `SoilVolumeConfig`:
  - `width`, `height`, `depth`: Voxel grid dimensions
  - `voxel_size`: World-space voxel size (meters)
  - `diffusion_rate`: Diffusion coefficient for nutrient mixing

**Returns**:
- `sample()`: Trilinearly interpolated `SoilVoxel`
- `width()/height()/depth()`: Grid dimensions
- `voxel_size()`: World-space voxel size

**Implementation Details**:
- Stores NPK, pH, and water per voxel
- Diffusion uses a 6-neighbor stencil with a scratch buffer
- Regeneration currently applies to nitrogen only (proxy for nutrients)

**Thread Safety**: Not thread-safe; mutations occur during `SoilSystem::tick()`.

**Storage**: Stored in `registry.ctx<SoilVolume>()` as a global service.

### BiomeMap

**Purpose**: Deterministic biome classification sampled by environment and gameplay systems.

**Public API**:
```cpp
explicit BiomeMap(const BiomeConfig& config, const Terrain& terrain);
BiomeId sample(double x, double z) const noexcept;
int width() const noexcept;
int height() const noexcept;
double cell_size() const noexcept;
```

**Parameters**:
- `BiomeConfig`:
  - `width_cells`, `height_cells`, `cell_size`: Match the terrain grid.
  - `seed`: Noise seed (defaults to terrain seed for consistency).
  - `biome_count`: Number of biomes to generate (2–4).
  - `octaves`, `base_frequency`, `lacunarity`, `gain`: Fractal noise controls.
  - `elevation_bias`, `alpine_threshold`: Optional bias toward alpine biome on high terrain.

**Usage**:
- `SoilGrid::regenerate_by_biome()` queries BiomeMap for per-cell regen rates.
- Plant seeding/growth logic (Phase 2) will gate species by biome mask bits.
- Viewer overlays colorize the terrain mesh by sampled biome.

**Thread Safety**: Read-only queries are thread-safe; construction occurs inside environment bootstrap.

**Storage**: Stored in `registry.ctx<BiomeMap>()`.

### WaterMap

**Purpose**: Encodes water depth, river channels, and shoreline distance derived from the terrain.

**Public API**:
```cpp
explicit WaterMap(const WaterConfig& config, const Terrain& terrain);
double depth(double x, double z) const noexcept;
double shore_distance(double x, double z) const noexcept;
bool is_water(double x, double z) const noexcept;
double water_level() const noexcept;
```

**Parameters**:
- `WaterConfig`:
  - Grid sizing matches terrain.
  - `water_level_percentile`: Percentile of terrain heights used as lake level.
  - `min_flow_accumulation`, `river_depth_scale`, `river_width_sigma`: Flow accumulation thresholds for carving rivers.
  - `shore_band_max`: Depth classified as shoreline (for plant spawning rules).

**Usage**:
- Plant spawning separates aquatic/shoreline/terrestrial species using `depth()` and `shore_distance()`.
- Viewer renders a translucent water surface using `water_level()` together with the depth field.
- Future climate systems can derive humidity or evaporation metrics from depth/shore distance.

**Thread Safety**: Read-only queries are thread-safe after construction.

**Storage**: Stored in `registry.ctx<WaterMap>()`.

### PlantSpatialIndex

**Purpose**: Uniform-grid spatial hash for efficient plant neighbor queries.

**Public API**:
```cpp
explicit PlantSpatialIndex(double cell_size = 4.0) noexcept;
void clear() noexcept;
void rebuild(entt::registry& registry);
template <typename Func>
void for_each_in_radius(entt::registry& registry, const Vec3& position, double radius, Func&& func) const;
```

**Parameters**:
- `cell_size`: Grid cell edge length in meters (default: 4.0m)

**Usage Pattern**:
```cpp
PlantSpatialIndex index{4.0};
index.rebuild(registry);
index.for_each_in_radius(registry, position, 2.0, [](entt::entity plant, double dist_sq) {
    // Process nearby plant
});
```

**Complexity**: 
- `rebuild()`: O(N) where N = plant count
- `for_each_in_radius()`: O(K) where K = plants in radius (amortized O(1) per plant)

**Thread Safety**: Not thread-safe; rebuilds occur during plant system ticks.

**Seeding Safeguards**:
- Plant seeding uses density checks via `PlantSpatialIndex` before spawning.
- A global plant cap (20k) throttles seeding probability near capacity.

### EnvironmentStats

**Purpose**: Maintains aggregated telemetry (per-biome biomass, per-species counts, soil mean, land coverage) for HUDs and logging.

**Structure**:
```cpp
struct EnvironmentStats {
    std::array<double, 4> biome_biomass;
    std::array<std::uint32_t, 5> species_counts;
    double total_biomass;
    double soil_mean;
    double land_fraction;
    void reset() noexcept;
};
```

**Usage**:
- `update_environment_stats(entt::registry&)` populates or refreshes the struct from the current registry state.
- `StatsSystem` calls the helper before emitting logs so headless runs capture up-to-date telemetry.
- The render client queries `EnvironmentStats` every frame to display soil averages, biome biomass, species counts, and land/ water coverage.

**Storage**: Stored in `registry.ctx<EnvironmentStats>()`; writes occur on the simulation thread when the helper is invoked.

## Systems

### EnvironmentBootstrapSystem

**Purpose**: One-time initialization of terrain and soil grid.

**Public API**:
```cpp
explicit EnvironmentBootstrapSystem(const TerrainConfig& terrain_config, const SoilConfig& soil_config);
void tick(SimulationContext& context) override;
std::string_view name() const override;
```

**Responsibilities**:
- Generate terrain heightfield from noise
- Create soil grid aligned to terrain
- Store both in `registry.ctx()`
- Runs once on first tick, then disables itself

**Thread Safety**: Not thread-safe; must run before other environment systems.

### SoilSystem

**Purpose**: Updates soil nutrient diffusion and regeneration each tick.

**Public API**:
```cpp
explicit SoilSystem(double diffusion_interval = 0.1) noexcept;
void tick(SimulationContext& context) override;
std::string_view name() const override;
```

**Responsibilities**:
- Call `SoilGrid::diffuse()` and `regenerate()` periodically
- Sub-samples diffusion to reduce cost (runs every N ticks)
- Maintains nutrient cycling for plant growth

**Performance**: O(W×H) where W×H = grid resolution; sub-sampled to reduce frequency.

### PlantGrowthSystem

**Purpose**: Updates plant energy from soil nutrients and light.

**Public API**:
```cpp
PlantGrowthSystem() = default;
void tick(SimulationContext& context) override;
std::string_view name() const override;
```

**Responsibilities**:
- Sample soil nutrients at plant position
- Increase plant energy: `energy += min(growth_rate * dt * soil_factor, max_energy)`
- Clamp energy to valid range

**Component Requirements**: `TransformComponent`, `PlantComponent`

**Data Flow**:
- Reads: `TransformComponent::position`, `PlantComponent::energy`, `SoilGrid`
- Writes: `PlantComponent::energy`

### PlantSeedingSystem

**Purpose**: Spawns new plants when parent plants accumulate sufficient energy.

**Public API**:
```cpp
PlantSeedingSystem() = default;
void tick(SimulationContext& context) override;
std::string_view name() const override;
```

**Responsibilities**:
- Check `PlantComponent::seed_timer >= seed_interval`
- If `energy >= seed_min_energy`: sample random position within `seed_radius`
- Test terrain slope and soil availability
- Spawn new plant entity with `seed_cost` deducted from parent
- Reset parent's `seed_timer`

**Component Requirements**: `TransformComponent`, `PlantComponent`, `PlantSeedParams`

**Spawn Logic**:
- Random angle and distance within `seed_radius`
- Terrain slope check (reject if too steep)
- Soil availability check (reject if depleted)
- `establish_probability` chance to succeed

### PlantCleanupSystem

**Purpose**: Removes dead plants after a delay period.

**Public API**:
```cpp
PlantCleanupSystem() = default;
void tick(SimulationContext& context) override;
std::string_view name() const override;
```

**Responsibilities**:
- Track plants with `PlantComponent::alive == false`
- After `cleanup_delay` seconds, destroy entity
- Prevents immediate removal to allow decomposition effects

**Component Requirements**: `PlantComponent`

### FeedingSystem

**Purpose**: Transfers energy from plants or prey to feeders based on diet.

**Public API**:
```cpp
FeedingSystem() = default;
void tick(SimulationContext& context) override;
std::string_view name() const override;
```

**Responsibilities**:
- Query `PlantSpatialIndex` for plants near each herbivore
- If distance <= `reach + plant.radius`: transfer energy from plants
- For carnivores, scan prey entities within `reach` and transfer energy on attack
- Transfer rate: `min(source.energy, intent.rate * dt)`
- Update predator `MetabolismComponent::energy` (clamped to max)
- Reduce plant/prey energy and mark plants dead if depleted
- Apply attack cooldowns via `CombatComponent`

**Component Requirements**:
- Herbivore: `TransformComponent`, `MetabolismComponent`, `FeedingIntent`, `DietComponent`
- Carnivore: `TransformComponent`, `MetabolismComponent`, `FeedingIntent`, `DietComponent`, `CombatComponent` (optional)
- Optional: `ActuationComponent` (attack gating)
- Plant: `TransformComponent`, `PlantComponent`

**Data Flow**:
- Reads: `PlantSpatialIndex`, positions, plant energy, prey energy, feeding intent, diet type
- Writes: `MetabolismComponent::energy`, `PlantComponent::energy`, prey `MetabolismComponent::energy`, `PlantComponent::alive`, `CombatComponent`

**Performance**: O(H × P_avg + C × N) where H = herbivores, C = carnivores, N = prey candidates.

## Components

### PlantComponent

**Structure**:
```cpp
struct PlantComponent {
    double energy;              // Current edible store
    double max_energy;          // Maximum capacity
    double growth_rate;         // Energy/sec from soil + light
    double radius;              // Edible/contact radius (m)
    double seed_interval;       // Seconds between seed attempts
    double seed_timer;          // Accumulates toward seeding
    double cleanup_delay;       // Delay before culling (s)
    bool alive;                 // Fast filter
};
```

**Data Contract**:
- **Read by**: FeedingSystem, PlantGrowthSystem, PlantSeedingSystem, PlantCleanupSystem
- **Written by**: PlantGrowthSystem (energy), PlantSeedingSystem (energy, timer), FeedingSystem (energy, alive), PlantCleanupSystem (entity destruction)
- **Thread Safety**: Not thread-safe

### PlantSeedParams

**Structure**:
```cpp
struct PlantSeedParams {
    double seed_min_energy;     // Parent threshold to seed
    double seed_cost;            // Energy removed from parent
    double seed_radius;          // Spawn radius (m)
    double establish_probability; // Chance to take root
};
```

**Data Contract**:
- **Read by**: PlantSeedingSystem
- **Written by**: Spawn systems (initialization)
- **Thread Safety**: Immutable after creation

### FeedingIntent

**Structure**:
```cpp
struct FeedingIntent {
    bool request_eat{true};     // For v1, always true when in range
    double reach{1.0};          // Max eat distance (m)
    double rate{5.0};           // Energy/sec transfer cap
};
```

**Data Contract**:
- **Read by**: FeedingSystem
- **Written by**: Brain systems (future), spawn systems (initialization)
- **Thread Safety**: Not thread-safe

### HerbivoreTag

**Structure**:
```cpp
struct HerbivoreTag {};
```

**Purpose**: Empty tag component marking entities that can consume plants.

**Data Contract**:
- **Read by**: FeedingSystem (for filtering herbivores)
- **Written by**: Spawn systems
- **Thread Safety**: Not applicable (tag only)

### CarnivoreTag

**Structure**:
```cpp
struct CarnivoreTag {};
```

**Purpose**: Empty tag component marking entities that can consume other entities.

**Data Contract**:
- **Read by**: FeedingSystem (for filtering carnivores)
- **Written by**: Spawn systems
- **Thread Safety**: Not applicable (tag only)

## Data Contracts

### Terrain ↔ Physics

**Contract**: Physics backend queries terrain height/normal for ground collisions.

**Data Flow**:
- Physics reads: `registry.ctx<Terrain>()` (if present)
- Fallback: Uses `ground_height` plane if terrain absent
- Query pattern: `terrain->height(x, z)` and `terrain->normal(x, z)`

**Lifetime**: Terrain created once during bootstrap, persists for simulation lifetime.

### Plants ↔ Soil

**Contract**: Plants sample soil nutrients for growth.

**Data Flow**:
- PlantGrowthSystem reads: `SoilVolume::sample(pos)` when available (fallback: `SoilGrid::sample(x, z)`)
- Future: Plants may consume soil nutrients (not implemented in v1)

**Synchronization**: Soil updates occur before plant growth in system order.

### Feeding ↔ Plants ↔ Metabolism

**Contract**: Feeding transfers energy from plants or prey based on diet.

**Data Flow**:
- FeedingSystem reads: `DietComponent::type`, `PlantComponent::energy`, prey `MetabolismComponent::energy`
- FeedingSystem writes: `PlantComponent::energy` or prey `MetabolismComponent::energy` (decreased)
- FeedingSystem writes: predator `MetabolismComponent::energy` (increased)
- Guarantee: Energy never < 0; predator energy clamped to `max_energy`

**Energy Conservation**: Energy transferred atomically; no loss during transfer.

### PlantSpatialIndex ↔ Plants

**Contract**: Index rebuilt each tick from current plant positions.

**Data Flow**:
- Plant systems rebuild index before feeding queries
- Index stores entity IDs, queries positions from registry

**Rebuild Frequency**: Once per tick (before feeding system runs).

## Performance Characteristics

### Time Complexity

- **Terrain::height()**: O(1) - bilinear interpolation
- **SoilGrid::diffuse()**: O(W×H) - five-point stencil
- **SoilVolume::diffuse()**: O(W×H×D) - 6-neighbor stencil
- **PlantGrowthSystem::tick()**: O(N) where N = plant count
- **PlantSeedingSystem::tick()**: O(N) with occasional spawns
- **FeedingSystem::tick()**: O(H × P_avg + C × N) where H = herbivores, C = carnivores, N = prey candidates

### Space Complexity

- **Terrain**: O(W×H) height samples
- **SoilGrid**: O(W×H) nutrient values + scratch buffer
- **SoilVolume**: O(W×H×D) voxel values + scratch buffer
- **PlantSpatialIndex**: O(N) where N = plant count

### Optimization Strategies

- Soil diffusion sub-sampled (runs every N ticks)
- Plant spatial index rebuilt incrementally (only on spawn/death in future)
- Bilinear interpolation for continuous queries (no discrete lookups)

## Testing Strategy

- **Unit tests**: Terrain height/normal correctness, soil diffusion mass conservation
- **Integration tests**: Plant growth from soil, feeding energy transfer, seeding spawns
- **Performance tests**: 10k plants + 1k herbivores at 60 Hz

## Future Extensions

- Biome-specific soil regeneration rates
- Plant species with different growth rates
- Decomposition returning nutrients to soil
- Seasonal effects on growth rates
- Water simulation affecting plant growth

## Related Documentation

- [Components Module](./components.md) - Component definitions
- [Physics System Module](./physics_system.md) - Terrain collision integration
- [Core Simulation Module](./core_simulation.md) - System execution infrastructure





