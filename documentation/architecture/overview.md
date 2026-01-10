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

2. **sim/components** - ECS component definitions
   - `TransformComponent`: Spatial positioning
   - `KinematicsComponent`: Velocity and forces
   - `MetabolismComponent`: Energy management
   - `GenomeHandleComponent`: Genetic data reference
   - `NameComponent`: Debug labeling
   - `PlantComponent`: Plant energy and lifecycle
   - `PlantSeedParams`: Plant reproduction parameters
   - `FeedingIntent`: Herbivore feeding behavior
   - `HerbivoreTag`: Herbivore marker
   - `CarnivoreTag`: Carnivore marker
   - `DietComponent`: Diet type (Herbivore/Carnivore/Omnivore)
   - `CombatComponent`: Attack cooldowns and pursuit state
   - `BrainComponent`: Neural controller metadata
   - `ActuationComponent`: Brain output commands (impulse, jump, eat, attack)
   - `ReproductionComponent`: Reproduction cooldown and policy
   - `TelemetryComponent`: Tracking kills, deaths, energy gained

3. **sim/physics** - Physics backends and pipeline
   - `physics_system.h/.cpp`: System façade delegating to backends
   - `physics/backend.h`: Backend interface
   - `physics/simple_backend.*`: Deterministic CPU backend
   - `physics/broad_phase|narrow_phase|solver.*`: Collision pipeline helpers
   - **Terrain Integration**: Heightfield collision via `Terrain` service

4. **sim/environment** - Living world systems
   - `environment/environment.h`: Terrain and soil grid definitions
   - `environment/environment_bootstrap.*`: One-time terrain/soil initialization
   - `environment/soil_system.*`: Nutrient diffusion and regeneration
   - `environment/plant_systems.*`: Plant growth, seeding, cleanup
   - `environment/feeding_system.*`: Energy transfer from plants to herbivores
   - **Services**: `Terrain` and `SoilGrid` stored in registry context

5. **sim/math** - Mathematical utilities
   - `Vec3`: 3D vector operations

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
- Plants sample nutrients for growth
- Stored as global service in `registry.ctx<SoilGrid>()`

**Plants**:
- Grow from soil nutrients
- Reproduce by seeding nearby locations
- Provide energy to herbivores via feeding
- Lifecycle managed by growth, seeding, and cleanup systems

**Feeding**:
- Herbivores consume nearby plants
- Energy transferred from `PlantComponent` to `MetabolismComponent`
- Spatial queries via `PlantSpatialIndex` for efficiency

**Predator-Prey System**:
- Carnivores require brain-controlled attack intent (`ActuationComponent.attack`)
- `CombatComponent` tracks attack cooldowns and pursuit state
- Carnivores drain energy from herbivores on successful attacks
- Energy transferred from prey's `MetabolismComponent` to predator's
- Attack success gated by:
  - `AttackIntent` from brain evolution
  - Attack cooldown timer
  - Reach distance from genome (`attack_reach`)
- Kills tracked in `TelemetryComponent.kill_count`
- Deaths from predation recorded as `DeathCause::Predation`
- Diet encoded in genome (`DietPreference` enum)
- Diet mutation rate ~1% creates occasional diet flips
- Diet distance penalty in speciation encourages reproductive isolation

### Spawn Workflow

- **Step 1 – Entity Creation**: Gameplay code creates an `entt::entity` and immediately assigns a `TransformComponent` containing the desired spawn pose.
- **Step 2 – Genome Binding**: Call `PhenotypeBuilder::build()` with the genome id and entity. The builder now preserves any pre-existing `TransformComponent` so the spawn pose survives the build. When no transform exists, it emplaces one at the origin.
- **Step 3 – Post-Build Systems**: Newly created entities already contain deterministic components (metabolism, brain, reproduction, etc.) and can immediately participate in standard system ticks.

This workflow ensures spawn systems own spatial placement while the phenotype builder owns the deterministic component graph.

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

