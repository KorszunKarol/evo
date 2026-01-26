# Data Contracts: Inter-Module Communication

## Overview

This document defines the data contracts between modules - what data flows between systems, what formats are used, and what guarantees are provided.

## Contract Types

### 1. Component Access Contracts

Define how systems read and write component data.

### 2. System Execution Contracts

Define the order and dependencies of system execution.

### 3. Registry Access Contracts

Define how systems access the ECS registry.

### 4. Context Propagation Contracts

Define how tick-scoped data flows through the system.

---

## Component Access Contracts

### TransformComponent Access

**Read Contract**:
- **Readers**: Physics systems, rendering systems, spatial queries, movement systems
- **Read Fields**: `position` (Vec3)
- **Read Frequency**: Every tick (physics), on-demand (queries)
- **Thread Safety**: Not thread-safe (single-threaded execution)

**Write Contract**:
- **Writers**: Physics systems (integration), spawn systems (initialization), movement systems (direct)
- **Write Fields**: `position` (Vec3)
- **Write Frequency**: Every tick (physics), once (spawn)
- **Thread Safety**: Not thread-safe
- **Validation**: None (caller responsible for valid values)

**Data Format**:
```cpp
struct TransformComponent {
    Vec3 position;  // Meters, world space, double precision
};
```

**Guarantees**:
- Position is always valid (finite values)
- Position represents entity center point
- Position updates atomically (no partial updates)

---

### KinematicsComponent Access

**Read Contract**:
- **Readers**: Physics systems, collision systems, movement systems
- **Read Fields**: `linear_velocity`, `accumulated_force`, `inverse_mass`
- **Read Frequency**: Every tick
- **Thread Safety**: Not thread-safe

**Write Contract**:
- **Writers**: 
  - Physics systems: `linear_velocity` (integration), `accumulated_force` (reset)
  - Force systems: `accumulated_force` (accumulation)
  - Spawn systems: All fields (initialization)
- **Write Frequency**: Every tick
- **Thread Safety**: Not thread-safe
- **Validation**: `inverse_mass >= 0.0` (caller responsibility)

**Data Format**:
```cpp
struct KinematicsComponent {
    Vec3 linear_velocity;      // m/s, double precision
    Vec3 accumulated_force;    // Newtons, double precision
    double inverse_mass;       // 1/kg, >= 0.0
};
```

**Force Accumulation Contract**:
1. **Accumulation Phase**: Systems add forces to `accumulated_force`
   - Pattern: `kinematics.accumulated_force += force_vector`
   - Order: Systems run in registration order
   - No guarantees: Order-dependent (forces applied sequentially)

2. **Integration Phase**: Physics system reads `accumulated_force`
   - Pattern: `acceleration = (gravity + accumulated_force) * inverse_mass`
   - Timing: After all force-accumulating systems run
   - Assumption: All forces accumulated before physics tick

3. **Reset Phase**: Physics system resets `accumulated_force`
   - Pattern: `accumulated_force = {0, 0, 0}`
   - Timing: After integration completes
   - Guarantee: Always reset to zero after use

**Guarantees**:
- `accumulated_force` reset to zero each tick (after physics)
- `inverse_mass == 0.0` implies static body (no integration)
- Velocity updates atomically (no partial updates)

---

### MetabolismComponent Access

**Read Contract**:
- **Readers**: Metabolism systems, action systems, death systems
- **Read Fields**: `energy`, `max_energy`, `basal_rate`
- **Read Frequency**: Every tick (metabolism), on-demand (actions)
- **Thread Safety**: Not thread-safe

**Write Contract**:
- **Writers**:
  - Metabolism systems: `energy` (consumption)
  - Feeding systems: `energy` (addition)
  - Spawn systems: All fields (initialization)
- **Write Frequency**: Every tick (metabolism), on-demand (feeding)
- **Thread Safety**: Not thread-safe
- **Validation**: `energy` should be in `[0, max_energy]` (caller responsibility)

**Data Format**:
```cpp
struct MetabolismComponent {
    double energy;      // Current energy, [0, max_energy]
    double max_energy;  // Capacity, > 0.0
    double basal_rate;  // Consumption per second, >= 0.0
};
```

**Energy Consumption Contract**:
1. **Basal Consumption**: Metabolism system consumes `basal_rate * dt` each tick
   - Pattern: `energy -= basal_rate * dt`
   - Clamping: `energy = std::max(0.0, energy)` after consumption
   - Frequency: Every tick

2. **Action Consumption**: Action systems consume energy before actions
   - Pattern: `if (energy >= cost) { energy -= cost; perform_action(); }`
   - Validation: Caller checks sufficient energy
   - Failure: Action skipped if insufficient energy

3. **Energy Addition**: Feeding systems add energy
   - Pattern: `energy = std::min(max_energy, energy + amount)`
   - Clamping: Never exceeds `max_energy`
   - Frequency: On-demand (when feeding occurs)

**Guarantees**:
- `energy` never negative (clamped to 0.0)
- `energy` never exceeds `max_energy` (clamped)
- `basal_rate` applied consistently each tick

---

### GenomeHandleComponent Access

**Read Contract**:
- **Readers**: Genome systems, reproduction systems, evolution systems
- **Read Fields**: `id` (std::uint64_t)
- **Read Frequency**: On-demand (genome lookup, reproduction)
- **Thread Safety**: Not thread-safe

**Write Contract**:
- **Writers**:
  - Spawn systems: `id` (initialization)
  - Reproduction systems: `id` (inheritance/modification)
- **Write Frequency**: Once per entity lifetime (typically)
- **Thread Safety**: Not thread-safe
- **Validation**: `id == 0` means invalid/uninitialized

**Data Format**:
```cpp
struct GenomeHandleComponent {
    std::uint64_t id;  // Genome identifier, 0 = invalid
};
```

**Genome Lookup Contract** (Future):
- `id == 0`: Invalid genome (entity has no genetic data)
- `id >= 1`: Valid genome identifier
- Lookup: `genome_storage.get(id)` returns genome data
- Lifetime: Genome data persists beyond entity lifetime

**Guarantees**:
- `id` is immutable after entity creation (unless reproduction)
- `id == 0` indicates no genome assigned
- Valid `id` values reference existing genome storage entries

---

## System Execution Contracts

### System Registration Contract

**Registration Phase**:
- **Timing**: Before first `tick()` call
- **Method**: `Scheduler::add_system(std::unique_ptr<ISystem>)`
- **Order**: Systems execute in registration order
- **Ownership**: Scheduler owns system instances
- **Thread Safety**: Single-threaded (setup phase)

**Guarantees**:
- Systems registered before first tick remain registered
- System order preserved throughout simulation
- Systems destroyed when scheduler destroyed

---

### System Execution Contract

**Execution Phase**:
- **Timing**: During `SimulationApp::tick()`
- **Method**: `Scheduler::tick_systems(context)`
- **Order**: Sequential (registration order)
- **Context**: Same `SimulationContext` passed to all systems
- **Thread Safety**: Single-threaded (execution phase)

**Execution Flow**:
```
1. SimulationApp::tick() called
2. SimulationContext created
3. For each system in registration order:
   a. system->tick(context) called
   b. System reads/writes components
   c. Continue to next system
4. Context destroyed
```

**Guarantees**:
- All systems execute exactly once per tick
- Systems execute in deterministic order
- Same context passed to all systems
- Exceptions propagate to caller (no exception handling)

**Dependencies**:
- **No explicit dependencies**: Systems run in registration order
- **Implicit dependencies**: Systems may depend on other systems' outputs
- **Example**: Physics system should run after force-accumulating systems
- **ReproductionSystem**: Must run after `MetabolismSystem` to ensure energy thresholds accurate
- **SpeciesIndexSystem**: Expensive O(N²) operation; consider running periodically rather than every tick for large populations

---

### Context Propagation Contract

**Context Creation**:
- **Creator**: `SimulationApp::tick()`
- **Lifetime**: Single tick duration
- **Contents**: Registry reference, fixed_dt, simulation_time
- **Thread Safety**: Not thread-safe

**Context Distribution**:
- **Path**: `SimulationApp` → `Scheduler` → `ISystem::tick()`
- **Sharing**: Same context instance passed to all systems
- **Modifications**: Systems modify registry via context reference

**Guarantees**:
- Context valid for entire tick duration
- Registry reference remains valid throughout tick
- Timing data (dt, time) immutable during tick

---

## Registry Access Contracts

### Component Query Contract

**View Creation**:
- **Method**: `registry.view<ComponentTypes...>()`
- **Returns**: View object for iteration
- **Complexity**: O(1) to create, O(N) to iterate
- **Thread Safety**: Not thread-safe

**Iteration Contract**:
- **Method**: `view.each(lambda)`
- **Lambda Signature**: `[](ComponentType&...) { ... }`
- **Order**: Entity iteration order (implementation-defined)
- **Modifications**: Components can be modified during iteration

**Guarantees**:
- View includes all entities with specified components
- Iteration order deterministic (but implementation-defined)
- Component modifications visible immediately

---

### Component Access Contract

**Get Component**:
- **Method**: `registry.get<Component>(entity)`
- **Returns**: `Component&` reference
- **Precondition**: Entity must have component attached
- **Exceptions**: Throws if component not attached
- **Thread Safety**: Not thread-safe

**Emplace Component**:
- **Method**: `registry.emplace<Component>(entity, args...)`
- **Returns**: `Component&` reference
- **Side Effects**: Attaches component to entity
- **Exceptions**: May throw `std::bad_alloc`
- **Thread Safety**: Not thread-safe

**Guarantees**:
- Component references valid until entity destroyed or component removed
- Component storage uses Structure of Arrays (SoA) layout
- Component access is O(1) amortized

---

## Data Flow Diagrams

### Tick Execution Flow

```
SimulationApp::tick()
    │
    ├─> Create SimulationContext
    │   ├─> registry reference
    │   ├─> fixed_dt
    │   └─> simulation_time
    │
    └─> Scheduler::tick_systems(context)
            │
            ├─> System1::tick(context)
            │   ├─> Read components via context.registry()
            │   └─> Write components via context.registry()
            │
            ├─> System2::tick(context)
            │   ├─> Read components (may read System1's writes)
            │   └─> Write components
            │
            └─> SystemN::tick(context)
                    └─> Read/write components
```

### Force Accumulation Flow

```
Force System → KinematicsComponent::accumulated_force += force
     │
     ├─> (Other force systems accumulate)
     │
     └─> Physics System
             ├─> Read accumulated_force
             ├─> Apply integration
             └─> Reset accumulated_force = {0, 0, 0}
```

### Energy Flow

```
Metabolism System
    ├─> Read MetabolismComponent::energy
    ├─> Consume: energy -= basal_rate * dt
    └─> Clamp: energy = max(0.0, energy)

Action System
    ├─> Read MetabolismComponent::energy
    ├─> Check: if (energy >= cost)
    └─> Consume: energy -= cost

Feeding System
    ├─> Read MetabolismComponent::energy, max_energy
    ├─> Read PlantComponent::energy
    ├─> Transfer: amount = min(plant.energy, rate * dt)
    ├─> Write: herbivore.energy = min(max_energy, energy + amount)
    └─> Write: plant.energy -= amount (mark dead if <= 0)
```

### Terrain ↔ Physics Contract

**Contract**: Physics backend queries terrain heightfield for ground collisions.

**Data Flow**:
```
Physics Backend (narrow-phase)
    ├─> Query: registry.ctx().find<Terrain>()
    │
    ├─> If terrain exists:
    │   ├─> terrain->height(x, z) → ground elevation
    │   ├─> terrain->normal(x, z) → surface normal
    │   └─> Apply collision with local normal
    │
    └─> If terrain absent:
        └─> Use flat ground_height plane
```

**Preconditions**:
- Terrain created by `EnvironmentBootstrapSystem` before physics runs
- Terrain stored in `registry.ctx<Terrain>()` with lifetime matching simulation

**Postconditions**:
- Bodies collide with terrain surface (not penetrate)
- Collision response uses local surface normal

**Thread Safety**: Terrain queries are read-only and thread-safe; physics runs on simulation thread.

### BiomeMap Service Contract

**Contract**: `EnvironmentBootstrapSystem` generates a deterministic biome classification (`BiomeMap`) and stores it in `registry.ctx<BiomeMap>()` for the duration of the simulation.

**Data Flow**:
- **Writer**: Environment bootstrap allocates `BiomeMap` after the terrain is created so both share the same seed and grid sizing.
- **Readers**:
  - `SoilSystem` / `SoilGrid::regenerate_by_biome()` to apply biome-specific regeneration rates.
  - Plant seeding and growth systems (Phase 2) to enforce biome masks per species.
  - Telemetry/viewer subsystems to colorize terrain or aggregate per-biome metrics.

**Guarantees**:
- `sample(x, z)` is O(1) using bilinear interpolation over cached samples.
- Same config + seed → identical biome layout (verified in `tests/sim/test_biome_water_determinism.cpp`).
- Biome count stays within the requested 2–4 range; each requested biome appears in the generated mask.

### WaterMap Service Contract

**Contract**: Environment bootstrap computes water level, lakes, rivers, and shoreline distances (`WaterMap`) and stores them in `registry.ctx<WaterMap>()`.

**Data Flow**:
- **Writer**: `initialize_environment()` derives water depth from the terrain percentile and flow accumulation, then precomputes a shoreline distance field.
- **Readers**:
  - Plant systems classify aquatic/shoreline/terrestrial zones using `depth()` and `shore_distance()`.
  - Soil telemetry, future climate cadence, and acceptance tests consume `water_level()` and land fraction metrics.
  - The render client draws a translucent water overlay using the map data.

**Guarantees**:
- `depth(x, z) >= 0` and matches `is_water(x, z)`.
- Shore distance equals zero in water and grows smoothly on land.
- Deterministic per seed; at least 20% of sampled cells remain above water (see tests).

### EnvironmentStats Service Contract

**Contract**: `EnvironmentStats` is a lightweight telemetry cache stored in the registry context and refreshed via `update_environment_stats()`.

**Data Flow**:
- **Writers**:
  - `StatsSystem` invokes `update_environment_stats()` at its logging cadence.
  - The render client also calls the helper every frame (since the viewer pauses the simulation loop).
- **Readers**:
  - HUD/telemetry code queries `registry.ctx<EnvironmentStats>()` to render per-biome biomass, species counts, soil mean, and land fraction.

**Guarantees**:
- Helper recomputes aggregates from live ECS data (plant view, soil grid, biome/water maps).
- Arrays are reset before each update to avoid drift.
- Consumers treat the struct as read-only between updates.

---

### SpeciesIndexContext Service Contract

**Contract**: `SpeciesIndexContext` provides access to the species clustering system stored in the registry context when `scenario.enable_species_index` is true.

**Read Contract**:
- **Readers**: Telemetry systems (species rollups), future evolutionary analysis systems
- **Read Fields**: `SpeciesIndexContext::system` (pointer)
- **Read Frequency**: On-demand (when species classification needed)
- **Thread Safety**: Not thread-safe (simulation thread only)

**Write Contract**:
- **Writers**: `setup_scenario()` during simulation initialization
- **Write Fields**: `SpeciesIndexContext::system` (pointer assignment)
- **Write Frequency**: Once during setup
- **Thread Safety**: Not thread-safe
- **Validation**: Context only exists when `scenario.enable_species_index == true`

**Data Format**:
```cpp
struct SpeciesIndexContext {
    SpeciesIndexSystem* system{nullptr};  // Pointer to species clustering system
};
```

**Gating**:
- **Scenario Flag**: `SimulationScenario::enable_species_index` controls context availability
- **When Disabled**: Context not added to registry; systems must check existence before access
- **When Enabled**: Context available for entire simulation lifetime

**Performance Considerations**:
- **Cost**: `SpeciesIndexSystem::tick()` is O(N²) where N = genome count (distance matrix computation)
- **Recommendation**: Run periodically rather than every tick for large populations
- **Access**: `system->get_species(genome_id)` provides O(1) average species lookup

**Guarantees**:
- Context pointer is valid for the lifetime of the simulation (when enabled)
- `system == nullptr` indicates species indexing disabled
- Species IDs are deterministic for a given threshold and genome set
- System maintains species count within configured target range via dynamic threshold adjustment

### Plants ↔ Soil Contract

**Contract**: Plants sample soil nutrients for growth.

**Data Flow**:
```
PlantGrowthSystem
    ├─> Read: TransformComponent::position
    ├─> Read: SoilGrid::sample(x, z) at plant position
    ├─> Compute: growth = growth_rate * dt * soil_factor
    └─> Write: PlantComponent::energy += growth (clamped to max_energy)
```

**Preconditions**:
- Soil grid exists in `registry.ctx<SoilGrid>()`
- Plant has `TransformComponent` and `PlantComponent`

**Postconditions**:
- Plant energy increases based on soil availability
- Energy clamped to `[0, max_energy]`

**Synchronization**: Soil updates occur before plant growth in system order.

### Feeding ↔ Plants ↔ Metabolism Contract

**Contract**: Feeding transfers energy from plants to herbivore metabolism.

**Data Flow**:
```
FeedingSystem
    ├─> Query: PlantSpatialIndex for plants near herbivore
    ├─> For each plant in radius:
    │   ├─> Check: distance <= reach + plant.radius
    │   ├─> Read: PlantComponent::energy, FeedingIntent::rate
    │   ├─> Compute: transfer = min(plant.energy, rate * dt)
    │   ├─> Write: MetabolismComponent::energy += transfer (clamped)
    │   └─> Write: PlantComponent::energy -= transfer
    └─> Mark plant dead if energy <= 0
```

**Preconditions**:
- Herbivore has `HerbivoreTag`, `FeedingIntent`, `MetabolismComponent`
- Plant has `PlantComponent` with `alive == true`
- `PlantSpatialIndex` rebuilt before feeding queries

**Postconditions**:
- Energy never < 0 (clamped)
- Herbivore energy never > max_energy (clamped)
- Plant marked dead if energy depleted

**Energy Conservation**: Energy transferred atomically; no loss during transfer.

**Thread Safety**: Not thread-safe; all mutations occur during feeding system tick.

---

## Contract Violations

### Undefined Behavior

- Accessing component that doesn't exist (use `try_get()` instead)
- Modifying registry from multiple threads
- Calling `tick()` before systems registered
- Passing `nullptr` to `add_system()`

### Precondition Violations

- `inverse_mass < 0.0` (should be >= 0.0)
- `energy < 0.0` or `energy > max_energy` (should be clamped)
- `fixed_dt <= 0.0` (should be positive)

### Postcondition Guarantees

- `accumulated_force` always reset after physics tick
- `energy` always in `[0, max_energy]` after metabolism tick
- `position` always finite after physics tick

---

## Future Contracts

### GPU Compute Contract (Planned)

- **Data Transfer**: Host → Device before kernel launch
- **Kernel Execution**: Batch processing on GPU
- **Data Transfer**: Device → Host after kernel completion
- **Synchronization**: CUDA streams for async execution

### Network Synchronization Contract (Planned)

- **Snapshot Format**: Serialized component state
- **Update Frequency**: Periodic snapshots (not every tick)
- **Delta Compression**: Only changed components transmitted
- **Consistency**: Deterministic simulation ensures consistency
---

## Telemetry Contracts

### Telemetry Context Access

**Read Contract**:
- **Readers**: Systems emitting events (FeedingSystem, MetabolismSystem, ReproductionSystem, SpeciesIndexSystem, BrainInferenceSystem, MotorSystem)
- **Read Fields**: `TelemetryContext::system`
- **Read Frequency**: Event-triggered
- **Thread Safety**: Not thread-safe (simulation thread only)

**Write Contract**:
- **Writers**: Scenario setup
- **Write Fields**: `TelemetryContext::system`
- **Write Frequency**: Once during setup
- **Thread Safety**: Not thread-safe

**Data Format**:
```cpp
struct TelemetryContext {
    TelemetrySystem* system;
};
```

**Guarantees**:
- Pointer is valid for the lifetime of the simulation
- Context exists only when telemetry is enabled

---

### Telemetry Output Files

**Events**:
- **File**: `telemetry/events.jsonl`
- **Format**: JSON Lines
- **Schema**:
```json
{
  "schema_version": 2,
  "run_id": "default",
  "type": "ENTITY_SPAWN",
  "sim_time": 12.34,
  "payload": { "entity_id": 42 }
}
```

**Rollups**:
- **File**: `telemetry/metrics.csv`
- **Format**: CSV
- **Schema**:
```
schema_version,run_id,sim_time,total_population,mean_energy,total_feeding_energy
```

**Species Rollups**:
- **File**: `telemetry/species_rollups.csv`
- **Format**: CSV
- **Schema**:
```
schema_version,run_id,sim_time,species_id,population,mean_energy
```

**Guarantees**:
- `schema_version` increments on breaking schema changes
- `run_id` is stable across all records within a run
