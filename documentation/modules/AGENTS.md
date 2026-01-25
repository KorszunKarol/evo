# AGENTS.md - Modules

**Generated:** 2025-01-25
**Parent:** `../AGENTS.md`

## OVERVIEW

Core simulation systems implementing creature behaviors, environmental dynamics, genetics pipeline, and evolution mechanics.

## STRUCTURE

```
modules/
├── core_simulation.md      # Simulation loop, scheduler, context
├── components.md           # All 28 ECS components
├── physics_system.md       # Physics engine and backend abstraction
├── physics_v1.md          # Physics v1 milestone
├── environment.md          # Terrain, biome, water, soil, plants
├── genome.md              # Genome storage, mutation, crossover
├── phenotype.md            # Building ECS entities from genomes
├── brain.md               # MLP and NEAT brain inference
├── math_types.md          # Vec3, Fixed64 math
├── render_client.md        # OpenGL rendering, terrain shading
├── combat.md              # Combat mechanics, state lifecycle
├── vision.md              # Raycasting, sensory perception
├── social_behavior.md       # Flocking, territoriality, pack hunting
├── spatial_indexing.md    # Spatial hash indices for queries
├── telemetry.md           # Metrics collection, logging
└── evolution.md           # Generational evolution, speciation
```

## WHERE TO START

### Core Framework
1. **`core_simulation.md`** - SimulationApp, Scheduler, SimulationContext
2. **`components.md`** - All component definitions and data contracts

### Behavior Pipeline
3. **`vision.md`** - VisionSystem: raycasting, sensory input
4. **`brain.md`** - BrainInferenceSystem: neural evaluation
5. **`social_behavior.md`** - SocialBehaviorSystem: flocking, territory
6. **`combat.md`** - Combat mechanics and state management

### Environment Integration
7. **`environment.md`** - Terrain, biome, water, soil systems
8. **`spatial_indexing.md`** - Spatial query optimization

### Evolution
9. **`evolution.md`** - EvolutionSystem, selection, reproduction
10. **`genome.md`** - Genome storage and operations
11. **`phenotype.md`** - PhenotypeBuilder: entity construction

## SYSTEM MAP

### Core Simulation (4 systems)

| System | File | Purpose | Dependencies |
|--------|------|---------|--------------|
| `SimulationApp` | `simulation_app.h` | Main entry | None |
| `Scheduler` | `scheduler.h` | System ordering | None |
| `PhysicsSystem` | `physics_system.cpp` | Collision, motion | Backend |
| `MetabolismSystem` | `metabolism_system.cpp` | Energy consumption | Components |
| `FitnessUpdateSystem` | `fitness_update_system.cpp` | Evolution metrics | Components |
| `StatsSystem` | `stats_system.cpp` | Population stats | Components |

### Behavior Systems (4 systems)

| System | File | Purpose | Dependencies |
|--------|------|---------|--------------|
| `VisionSystem` | `vision_system.cpp` | Sensory input | PhysicsBackend, SpatialIndex |
| `BrainInferenceSystem` | `brain_inference_system.cpp` | Neural eval | Brain, Actuation |
| `SocialBehaviorSystem` | `social_behavior_system.cpp` | Social behavior | Vision, SocialSignals |
| `MotorSystem` | `motor_system.cpp` | Movement | Actuation |

### Evolution Systems (3 systems)

| System | File | Purpose | Dependencies |
|--------|------|---------|--------------|
| `EvolutionSystem` | `evolution_system.cpp` | Generation trigger | Fitness, GenomeStorage |
| `SpeciesIndexSystem` | `species_index_system.cpp` | Speciation | GenomeOps |
| `TraitAnalysisSystem` | `trait_analysis_system.cpp` | Trait stats | All genomes |

### Environment Systems (5+ systems)

| System | File | Purpose | Dependencies |
|--------|------|---------|--------------|
| `SoilSystem` | `soil_system.cpp` | Diffusion, regen | SoilGrid/Volume, BiomeMap |
| `PlantGrowthSystem` | `plant_systems.cpp` | Energy gain | SoilGrid, PlantSpatialIndex |
| `PlantSeedingSystem` | `plant_systems.cpp` | Spawning | SpatialIndex, SpeciesRegistry |
| `FeedingSystem` | `feeding_system.cpp` | Energy transfer | SpatialIndex, PhysicsBackend |
| `DecompositionSystem` | `decomposition_system.cpp` | Corpse decay | CorpseComponent, Soil |

## DATA FLOW

```
GenomeStorage
    ↓
PhenotypeBuilder (spawn entities)
    ↓
VisionSystem (sensory input)
    ↓
BrainInferenceSystem (cognition)
    ↓
SocialBehaviorSystem (social signals, combat)
    ↓
MotorSystem (actuation)
    ↓
PhysicsSystem (motion, collision)
    ↓
MetabolismSystem (energy consumption)
    ↓
FitnessUpdateSystem (fitness tracking)
    ↓
EvolutionSystem (generation trigger)
    ↓
    ↓ (cycle repeats)
```

## CONVENTIONS

### System Implementation

- **Inherit from ISystem** - `class MySystem : public ISystem`
- **Implement `tick()`** - `void tick(SimulationContext&) override`
- **Implement `name()`** - `std::string_view name() const noexcept override`
- **No state in constructor** - Systems registered via scheduler, not constructed inline

### Component Usage

- **Read via view** - `registry.view<Component1, Component2>()`
- **Write via get** - `registry.get<Component>(entity)` for mutation
- **Create via emplace** - `registry.emplace<Component>(entity, args...)`
- **Destroy via destroy** - `registry.destroy(entity)`

### Registry Context

- **Environment services** - `registry.ctx().emplace<Type>(service)`
- **Access services** - `registry.ctx().get<Type>()`

## COMMANDS

### Add New System

1. Create header in `sim/include/evolution/sim/my_system.h`
2. Implement ISystem interface
3. Register in `core/system_slices.cpp` or `simulation_app.cpp`

### Run Specific Systems Only

```cpp
// Create scheduler, add only desired systems
Scheduler custom_scheduler;

custom_scheduler.add_system(std::make_unique<PhysicsSystem>(backend));
custom_scheduler.add_system(std::make_unique<MetabolismSystem>());
// ... other systems

SimulationApp app(std::move(custom_scheduler));
```

## CODE CONVENTIONS

### Language Style

- **C++20** features: `[[nodiscard]]` for return values, `noexcept` for non-throwing functions
- **Const correctness**: Mark read-only operations with `const` where applicable
- **RAII patterns**: Use smart pointers, never `new`/`delete` without ownership
- **Move semantics**: Pass large objects by move when possible

### Naming Conventions

**Components:**
- `*Component` suffix (e.g., `TransformComponent`, `MetabolismComponent`)
- Use PascalCase for component names
- Use snake_case for component member variables (e.g., `linear_velocity`, `accumulated_force`)

**Systems:**
- `*System` suffix (e.g., `PhysicsSystem`, `VisionSystem`)
- Use PascalCase for class names
- Snake_case for member variables and local variables

**Functions:**
- Verb + noun (e.g., `get_genome`, `build_phenotype`)
- Snake_case for all names

**Types:**
- PascalCase for classes, structs, enums
- UPPERCASE for constants/macros (e.g., `MAX_ENTITIES`, `DEFAULT_DT`)

### Code Patterns

**ECS Usage:**
```cpp
// CORRECT: Use component views for iteration
auto view = registry.view<TransformComponent, KinematicsComponent>();
view.each([](auto entity, TransformComponent& transform, KinematicsComponent& kin) {
    // Process entity
});

// INCORRECT: Manual iteration over entities
for (auto entity : entities) {
    auto& transform = registry.get<TransformComponent>(entity); // O(N) lookup!
}
```

**Component Access:**
```cpp
// CORRECT: Direct access when you have entity reference
auto& transform = registry.get<TransformComponent>(entity);

// INCORRECT: Searching entity when you already have reference
```

**System Registration:**
```cpp
// Register systems in dependency order
scheduler.add_system(std::make_unique<PhysicsSystem>(backend));
scheduler.add_system(std::make_unique<MetabolismSystem>());
// Physics runs BEFORE metabolism (important for energy costs)
```

**Memory Management:**
```cpp
// Prefer component references, avoid copies
for (auto entity : view) {
    auto& energy = registry.get<MetabolismComponent>(entity);
    energy.amount += 10.0; // Direct mutation, no copy
}
```

## PROJECT-SPECIFIC RULES

### Determinism Requirements

- **RNG Usage**: ALL random operations must use derived seeds
```cpp
// CORRECT: Deterministic seed derivation
std::uint64_t seed = derive_seed(global_seed, genome_seed, OP_TAG_MUTATE, counter);
auto result = rng.normal(seed);

// INCORRECT: Global random
auto result = std::uniform_real_distribution(0.0, 1.0)(gen_);
```

- **Genome IDs**: Always content hashes, never assigned
- **System Order**: Fixed order enforced by Scheduler
- **Physics**: Fixed iteration count for solver stability

### Component Rules

- **No Logic in Components**: Components are pure data structs
- **State Goes in Systems**: All game logic in system `tick()` methods
- **Immutable After Creation**: Components like `GenomeHandleComponent` should never be mutated
```cpp
// INCORRECT: Modifying genome handle
registry.get<GenomeHandleComponent>(entity).generation = 5; // WRONG!

// CORRECT: Use separate component for runtime state
registry.get<RuntimeStrategyComponent>(entity).generation = 5;
```

### Error Handling

- **Validation First**: Check preconditions before operations
```cpp
// Check genome exists
auto* genome = storage.get(genome_id);
if (!genome) {
    spdlog::error("Genome not found: {}", genome_id);
    return;
}
```

- **Graceful Degradation**: Handle missing services safely
```cpp
// Check for physics backend
if (!registry.ctx().contains<IPhysicsBackend>()) {
    spdlog::warn("Physics backend not available, using simplified physics");
}
```

### Performance Patterns

- **Spatial Queries**: Always use spatial indices, never O(N²) iteration
```cpp
// CORRECT: Spatial index query
auto nearby = plant_spatial_index.for_each_in_radius(registry, position, radius, callback);

// INCORRECT: Brute force iteration
for (auto& plant : registry.view<PlantComponent>()) {
    double dist = distance(creature.position, plant.position);
    if (dist < radius) callback(plant);
}
```

- **Batch Operations**: Minimize registry calls
```cpp
// CORRECT: Batch entity creation
std::vector<entt::entity> new_entities;
for (int i = 0; i < 100; ++i) {
    new_entities.push_back(registry.create());
}
// Process all at once
for (auto entity : new_entities) {
    // Add components
}
```

## TESTING CONVENTIONS

### Test Organization

- **Directory Structure**: `tests/sim/`, `tests/physics/`, `tests/genetics/`, `tests/client/`
- **File Naming**: `test_<module>_<feature>.cpp` (e.g., `test_physics_collision.cpp`)
- **Test Naming**: `TEST(<name>, <description>)` for GoogleTest

### Test Fixtures

Located in `tests/test_fixtures.h`:
- `SimulationTestFixture` - Common setup for simulation tests
- `PhysicsTestFixture` - Physics backend testing
- `GeneticsTestFixture` - Genome operations testing

### Common Test Patterns

```cpp
TEST(VisionTest, RaycastReturnsCorrectHitType) {
    // Given: setup fixture with entities
    // When: raycasting
    // Then: verify hit type classification
    EXPECT_EQ(result.type, VisionHitType::Plant);
}
```

## ANTI-PATTERNS (DO NOT USE)

### Code Anti-Patterns

- **NO raw pointers** without ownership semantics
- **NO `delete`** on stack-allocated objects
- **NO global mutable state** in systems
- **NO system-to-system direct calls** (use components for communication)
- **NO exceptions in hot paths** (use error codes instead)
- **NO floating-point accumulation** in physics or determinism-critical paths

### Specific Prohibitions

❌ **NEVER modify `GenomeHandleComponent`** - genome IDs must stay constant
❌ **NEVER use global `std::rand` or `std::uniform_*`** - use PCG32 with derived seeds
❌ **NEVER call `registry.get<>()` in inner loops** - use component views
❌ **NEVER skip spatial index validation** - O(N²) loops are unacceptable
❌ **NEVER use `as any` or `@ts-ignore`** - type safety is mandatory
❌ **NEVER suppress errors with `catch(...)`** - let errors propagate appropriately

### Architecture Anti-Patterns

❌ **NEVER put game logic in components** - components are pure data
❌ **NEVER create singleton systems** - systems should be stateless
❌ **NEVER use inheritance for code reuse** - use composition
❌ **NEVER make systems dependent on concrete types** - use interfaces
❌ **NEVER couple systems tightly** - communicate via components only

## CONVENTIONS

### Documentation

- **Markdown** with consistent headers (##, ###, -, |)
- **Code blocks** with language tags (cpp, bash, python)
- **Cross-references** in Related Documentation sections
- **API tables** with Parameters/Returns/Exceptions/Complexity

### Code

- **C++20** with EnTT ECS framework
- **FlatBuffers** for genome serialization
- **spdlog** for logging
- **PCG32** RNG for determinism

### Testing

- **GoogleTest** for unit tests
- **Test fixtures** in `tests/` directories
- **Coverage**: sim/, physics/, genetics/, client/

1. Create header in `sim/include/evolution/sim/my_system.h`
2. Implement ISystem interface
3. Register in `core/system_slices.cpp` or `simulation_app.cpp`
4. Add documentation to appropriate module file

### Run Specific Systems Only

```cpp
// Create scheduler, add only desired systems
Scheduler custom_scheduler;

custom_scheduler.add_system(std::make_unique<PhysicsSystem>(backend));
custom_scheduler.add_system(std::make_unique<MetabolismSystem>());
// ... other systems

SimulationApp app(std::move(custom_scheduler));
```

## NOTES

### System Execution Order (Complete)

1. `EnvironmentBootstrapSystem` (first tick only)
2. `PlantSpatialSystem` (rebuild index)
3. `CreatureSpatialIndexSystem` (rebuild index)
4. `SoilSystem` (diffusion, regeneration)
5. `PlantGrowthSystem` (energy gain)
6. `VisionSystem` (sensory input)
7. `BrainInferenceSystem` (cognition)
8. `SocialBehaviorSystem` (social signals, combat)
9. `MotorSystem` (actuation forces)
10. `PhysicsSystem` (collision detection)
11. `FeedingSystem` (energy transfer)
12. `MetabolismSystem` (energy consumption)
13. `FitnessUpdateSystem` (fitness integration)
14. `DecompositionSystem` (corpse decay)
15. `EvolutionSystem` (generation trigger, every 60s)
16. `SpeciesIndexSystem` (species clustering)
17. `TraitAnalysisSystem` (trait statistics)
18. `TelemetrySystem` (metrics aggregation)

### Performance Notes

- **Hot path**: `PhysicsSystem::step()` - collision detection
- **Expensive**: `VisionSystem` - raycasting O(R × E) where R = rays, E = entities
- **Optimized**: `SocialBehaviorSystem` - spatial index for neighbor queries
- **Time-sliced**: `EvolutionSystem` - runs every 60 seconds

---

**Last Updated:** 2025-01-25
