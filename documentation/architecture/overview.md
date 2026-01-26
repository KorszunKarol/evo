# Evolution Simulation - Architecture Overview

## System Architecture

The Evolution Simulation is built on a **headless simulation server** architecture designed for deterministic, reproducible evolutionary experiments. The system follows an **Entity Component System (ECS)** pattern with a modular, pluggable system architecture.

### Core Design Principles

1. **Deterministic Simulation**: Fixed timestep ensures reproducibility across runs
2. **Modular Systems**: Independent systems operate on shared ECS state
3. **Separation of Concerns**: Simulation logic separate from rendering (future)
4. **Performance-First**: Cache-friendly data layouts, prepared for GPU acceleration
5. **Extensibility**: Easy to add new systems, components, and behaviors

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    SimulationApp                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │   Registry   │  │  Scheduler   │  │   Context    │     │
│  │  (EnTT ECS)  │  │  (Systems)   │  │  (Tick Info) │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘
                          │
                          │ tick()
                          ▼
        ┌─────────────────────────────────────┐
        │         System Execution            │
        │  ┌──────────┐  ┌──────────┐       │
        │  │ Physics  │  │  (More   │  ...   │
        │  │ System   │  │ Systems) │       │
        │  └──────────┘  └──────────┘       │
        └─────────────────────────────────────┘
                          │
                          │ reads/writes
                          ▼
        ┌─────────────────────────────────────┐
        │      Component Storage (ECS)        │
        │  Transform │ Kinematics │ Metabolism│
        │  Plant │ FeedingIntent │ Brain │ ...│
        └─────────────────────────────────────┘
                          │
                          │ services
                          ▼
        ┌─────────────────────────────────────┐
        │      Global Services (Context)      │
        │  Terrain │ SoilGrid │ (Future)      │
        └─────────────────────────────────────┘
```

## Module Structure

### Core Modules

1. **sim/core** - Foundation layer
   - `SimulationApp`: Main application façade
   - `Scheduler`: System execution manager
   - `SimulationContext`: Tick-scoped state access

2. **sim/telemetry** - Observability and analytics output
   - `TelemetrySystem`: Event and rollup telemetry
   - `TelemetryContext`: Registry access for emitters

3. **sim/components** - ECS component definitions
   - `TransformComponent`: Spatial positioning
   - `KinematicsComponent`: Velocity and forces
   - `MetabolismComponent`: Energy management
   - `GenomeHandleComponent`: Genetic data reference
   - `NameComponent`: Debug labeling
   - `PlantComponent`: Plant energy and lifecycle
   - `PlantSeedParams`: Plant reproduction parameters
   - `FeedingIntent`: Feeding behavior
   - `DietComponent`: Herbivore/carnivore routing
   - `HerbivoreTag`: Herbivore marker
   - `CarnivoreTag`: Carnivore marker
   - `CombatComponent`: Predator cooldown and targeting metadata
   - `BrainComponent`: Neural controller metadata
   - `ActuationComponent`: Brain output commands
   - `ReproductionComponent`: Reproduction cooldown and policy

4. **sim/physics** - Physics backends and pipeline
   - `physics_system.h/.cpp`: System façade delegating to backends
   - `physics/backend.h`: Backend interface
   - `physics/simple_backend.*`: Deterministic CPU backend
   - `physics/broad_phase|narrow_phase|solver.*`: Collision pipeline helpers
   - **Terrain Integration**: Heightfield collision via `Terrain` service

5. **sim/environment** - Living world systems
   - `environment/environment.h`: Terrain and soil grid/volume definitions
   - `environment/environment_bootstrap.*`: One-time terrain/soil initialization
   - `environment/soil_system.*`: Nutrient diffusion and regeneration
   - `environment/plant_systems.*`: Plant growth, seeding, cleanup
   - `environment/feeding_system.*`: Energy transfer from plants or prey
   - **Services**: `Terrain`, `SoilGrid`, and `SoilVolume` stored in registry context

6. **sim/math** - Mathematical utilities
   - `Vec3`: 3D vector operations

7. **sim/reproduction_system** - Mate selection and reproduction
   - `ReproductionSystem`: Preference-driven mate selection with crossover/mutation
   - Uses PreferenceNet when available, falls back to energy/cooldown rules
   - Creates offspring genomes and builds phenotype entities

8. **sim/species_index_system** - Species clustering and indexing
   - `SpeciesIndexSystem`: Clusters genomes into species using compatibility distance
   - Maintains dynamic threshold to keep species count in target range
   - Provides species ID lookup for genomes

9. **sim/scenario** - Simulation configuration and initialization
   - `SimulationScenario`: High-level experiment configuration
   - `setup_scenario()`: Configures simulation app with systems and services
   - `seed_initial_population()`: Spawns initial population from randomly generated genomes

## Data Flow

### Tick Execution Flow

```
1. SimulationApp::tick() called
   │
   ├─> begin_tick() - Logging/metrics hooks
   │
   ├─> Create SimulationContext
   │   ├─> Registry reference
   │   ├─> Fixed timestep (dt)
   │   └─> Current simulation time
   │
   ├─> Scheduler::tick_systems(context)
   │   │
   │   └─> For each registered system:
   │       ├─> ISystem::tick(context)
   │       └─> System reads/writes components via registry
   │
   └─> end_tick() - Update time counters
```

### Component Access Pattern

Systems access components through EnTT views:

```cpp
// System reads components
auto view = registry.view<TransformComponent, KinematicsComponent>();
view.each([](TransformComponent& transform, KinematicsComponent& kinematics) {
    // Read and modify components
});
```

## Thread Safety Model

**Current State**: Single-threaded execution
- All systems execute sequentially on main simulation thread
- No concurrent access to registry
- Future: Parallel system execution planned

**Thread Safety Guarantees**:
- `SimulationApp`: Not thread-safe (single-threaded use)
- `Scheduler`: Not thread-safe (setup phase only)
- `ISystem`: Implementations must handle own synchronization
- Components: Not thread-safe (ECS-level synchronization needed)

## Performance Characteristics

### Time Complexity

- **SimulationApp::tick()**: O(S) where S = number of systems
- **Scheduler::tick_systems()**: O(S)
- **PhysicsSystem::tick()**: O(N + P) where N = body count, P = contact pairs
- **SimplePhysicsBackend::step()**: O(N + P) per tick (sequential impulses)

### Space Complexity

- **Registry**: O(E × C) where E = entities, C = average components per entity
- **Scheduler**: O(S) for system storage
- **Systems**: O(1) per system instance

### Cache Performance

- EnTT uses **Structure of Arrays (SoA)** layout
- Components stored contiguously in memory
- Iteration patterns optimized for cache locality

## Environment Systems

The environment module provides a living world with terrain, soil nutrients, plants, and feeding interactions:

**Terrain**:
- Deterministic heightfield generated from Perlin noise
- Stored as global service in `registry.ctx<Terrain>()`
- Physics backend automatically uses terrain for ground collisions

**Soil**:
- 2D nutrient grid with diffusion and regeneration
- 3D soil volume for volumetric nutrient sampling
- Plants sample nutrients for growth (prefers `SoilVolume` when present)
- Stored as global services in `registry.ctx<SoilGrid>()` and `registry.ctx<SoilVolume>()`

**Plants**:
- Grow from soil nutrients
- Reproduce by seeding nearby locations
- Provide energy to herbivores via feeding
- Lifecycle managed by growth, seeding, and cleanup systems

**Feeding**:
- Herbivores consume nearby plants; carnivores consume nearby prey
- Energy transferred from plants/prey to `MetabolismComponent`
- Spatial queries via `PlantSpatialIndex` for plant lookups
- Carnivore attacks gated by `ActuationComponent::attack` and `CombatComponent`

## Extension Points

### Adding a New System

1. Implement `ISystem` interface
2. Register with `Scheduler::add_system()`
3. Access components via `SimulationContext::registry()`
4. Access services via `registry.ctx<ServiceType>()`

### Adding a New Component

1. Define component struct in `components.h`
2. Systems query via `registry.view<ComponentType>()`
3. Entities attach via `registry.emplace<ComponentType>(entity, ...)`

### Adding a New Service

1. Define service class (e.g., `Terrain`, `SoilGrid`)
2. Store in registry context: `registry.ctx().emplace<ServiceType>(...)`
3. Access via: `registry.ctx<ServiceType>()` or `registry.ctx().find<ServiceType>()`
4. Lifetime: Persists for simulation duration (or until explicitly removed)

### Adding a New Module

1. Create module directory under `sim/`
2. Add headers to `sim/include/evolution/sim/`
3. Add sources to `sim/src/`
4. Update `CMakeLists.txt` with new files

## Future Architecture Plans

### Phase 2: GPU Integration
- CUDA compute kernels for neural network inference
- Batch processing of creature brains
- GPU-accelerated physics (optional)

### Phase 3: Rendering Client
- Separate rendering process/thread
- State synchronization via snapshots
- Unreal Engine integration

### Phase 4: Distributed Simulation
- Multi-process simulation
- Spatial partitioning across processes
- Network synchronization

## Dependencies

### External Libraries

- **EnTT v3.12.2**: ECS framework (header-only)
- **spdlog v1.13.0**: Logging library
- **FlatBuffers v24.3.25**: Serialization (future genome storage)

### Build System

- **CMake 3.20+**: Build configuration
- **g++ 11.4.0+**: C++20 compiler
- **C++20 Standard**: Required language features

## File Organization

```
sim/
├── include/evolution/sim/     # Public headers
│   ├── components.h            # ECS component definitions
│   ├── math_types.h            # Vec3 and math utilities
│   ├── physics/                # Physics interface + helpers
│   │   ├── backend.h
│   │   ├── simple_backend.h
│   │   ├── broad_phase.h
│   │   ├── narrow_phase.h
│   │   ├── solver.h
│   │   └── physics_types.h
│   ├── physics_system.h        # Physics system interface
│   ├── scheduler.h             # System scheduling
│   ├── simulation_app.h        # Main application
│   └── simulation_context.h    # Tick context
├── src/                        # Implementation files
│   ├── main.cpp                # Entry point
│   ├── physics/                # Physics backend implementations
│   │   ├── broad_phase.cpp
│   │   ├── narrow_phase.cpp
│   │   ├── solver.cpp
│   │   └── simple_backend.cpp
│   ├── physics_system.cpp      # System façade
│   ├── scheduler.cpp
│   └── simulation_app.cpp
└── BUILD.bazel                  # Bazel build config (alternative)
```

## Design Decisions

### Why ECS?
- **Performance**: Cache-friendly component storage
- **Flexibility**: Entities can have arbitrary component combinations
- **Scalability**: Handles thousands of entities efficiently

### Why Fixed Timestep?
- **Determinism**: Reproducible simulation results
- **Stability**: Prevents numerical instability
- **Evolution**: Consistent fitness evaluation across runs

### Why Headless First?
- **Separation**: Simulation logic independent of rendering
- **Testing**: Easier to test and validate
- **Performance**: No rendering overhead during development
- **Distribution**: Can run on servers without GPUs

## Related Documentation

- [Module Documentation](./modules/) - Detailed module specs
- [API Reference](./api/) - Complete API documentation
- [Data Contracts](./data-contracts/) - Inter-module data flow
