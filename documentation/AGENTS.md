# AGENTS.md - Documentation Index

**Generated:** 2025-01-25
**Commit:** Main branch
**Branch:** docs-update

## OVERVIEW

Evolution simulation of agent-based ecosystem with neural-network-driven creatures, procedural terrain, generational evolution, and spatial environmental modeling.

**Tech Stack:** C++20, EnTT (ECS), FlatBuffers (genome serialization), SimplePhysicsBackend (CPU physics), OpenGL (rendering), spdlog (logging)

## STRUCTURE

```
documentation/
├── modules/                    # System and component modules
│   ├── core_simulation.md      # Simulation loop, scheduler, context
│   ├── components.md           # All ECS components (28 documented)
│   ├── physics_system.md       # Physics engine and backend abstraction
│   ├── physics_v1.md          # Physics v1 milestone achievements
│   ├── environment.md          # Terrain, biome, water, soil, plants
│   ├── genome.md              # Genome storage, mutation, crossover
│   ├── phenotype.md            # Building ECS entities from genomes
│   ├── brain.md               # MLP and NEAT brain inference
│   ├── math_types.md          # Vec3, Fixed64 math utilities
│   ├── render_client.md        # OpenGL rendering, terrain shading
│   ├── combat.md              # COMBAT: Combat mechanics, state lifecycle (NEW)
│   ├── vision.md              # VISION: Raycasting, sensory perception (NEW)
│   ├── social_behavior.md       # SOCIAL: Flocking, territoriality, pack hunting (NEW)
│   ├── spatial_indexing.md    # SPATIAL: Creature/Plant spatial indices (NEW)
│   ├── telemetry.md           # TELEMETRY: Metrics collection, logging (NEW)
│   └── evolution.md           # EVOLUTION: Generational evolution, speciation (NEW)
├── architecture/
│   ├── overview.md             # High-level architecture description
│   ├── actual_vs_documented.md  # ARCHITECTURE: Documented vs actual codebase (NEW)
│   └── data-contracts/         # Inter-module data flow contracts
│       ├── inter_module_contracts.md  # System-to-system contracts
│       └── genetics.md              # Genetics module contracts
├── api/
│   └── function_reference.md  # Complete API signatures for major classes
├── researches/
│   └── Ecosystem Simulation Model Research.md  # Academic research on v2.0 ecosystem
├── project_scope.md           # Project goals and boundaries
├── requirements.md            # Technical requirements
├── docs_gap_analysis.md      # DOCUMENTATION GAP ANALYSIS (NEW)
├── module_comparison.md        # MODULE COMPARISON: Documented vs actual (NEW)
├── undocumented_features.md    # UNDOCUMENTED FEATURES: Catalog of missing items (NEW)
└── README.md                 # Documentation index and overview
```

## WHERE TO START

### For Understanding the Codebase

1. **`architecture/overview.md`** - High-level system architecture
2. **`modules/core_simulation.md`** - Core simulation framework
3. **`modules/components.md`** - All ECS components

### For Adding New Features

1. **`modules/brain.md`** - How to add new brain types
2. **`modules/phenotype.md`** - How to build phenotypes
3. **`modules/genome.md`** - How to extend genetics

### For Performance Tuning

1. **`modules/physics_system.md`** - Physics backend optimization
2. **`modules/spatial_indexing.md`** - Spatial query optimization
3. **`modules/telemetry.md`** - Profiling and metrics

## CODE MAP

### Core Simulation

| Symbol | Type | Location | Role |
|--------|------|----------|------|
| `SimulationApp` | class | `sim/include/evolution/sim/simulation_app.h` | Main simulation entry |
| `Scheduler` | class | `sim/include/evolution/sim/scheduler.h` | System execution order |
| `SimulationContext` | struct | `sim/include/evolution/sim/simulation_context.h` | Tick-scoped state |

### ECS Components (28 total)

| Component | Purpose | Key Fields |
|-----------|---------|-------------|
| `TransformComponent` | Position, rotation | `position` (Vec3) |
| `KinematicsComponent` | Velocity, forces | `linear_velocity`, `accumulated_force` |
| `ColliderComponent` | Shape, material | `type` (sphere/AABB/capsule) |
| `MetabolismComponent` | Energy tracking | `energy`, `max_energy`, `basal_rate` |
| `FitnessComponent` | Evolution metrics | `age_seconds`, `energy_int_accum`, `offspring_count` |
| `BrainComponent` | Neural controller | `kind` (MLP/NEAT), `input_count`, `output_count` |
| `ActuationComponent` | Motor commands | `impulse_x`, `impulse_z`, `jump`, `eat` |
| `VisionComponent` | Sensory perception | `fov_radians`, `ray_count`, `max_range` |
| `SocialSignalsComponent` | Flocking inputs | `cohesion`, `alignment`, `separation` |
| `TerritoryComponent` | Territoriality | `center`, `radius` |
| `CombatComponent` | Combat state | `attack_cooldown`, `target`, `pursuing` |

### Systems (19 documented)

| System | Purpose | Reads | Writes |
|--------|---------|-------|--------|
| `PhysicsSystem` | Collision, motion | Transform, Kinematics, Collider |
| `MetabolismSystem` | Energy consumption | MetabolismComponent |
| `BrainInferenceSystem` | Neural evaluation | Brain, Actuation |
| `VisionSystem` | Sensory input | Vision, Transform |
| `SocialBehaviorSystem` | Social behavior | SocialSignals, Combat, Territory |
| `EvolutionSystem` | Generational change | Fitness, GenomeHandle |
| `TelemetrySystem` | Metrics logging | TelemetryComponent |

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
- **State Goes in Systems**: All game logic in system tick() methods
- **Immutable After Creation**: Components like GenomeHandleComponent should never be mutated
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
- **NO `delete` on stack-allocated objects**
- **NO global mutable state** in systems
- **NO system-to-system direct calls** (use components for communication)
- **NO exceptions in hot paths** (use error codes instead)
- **NO floating-point accumulation** in physics or determinism-critical paths

### Specific Prohibitions

❌ **NEVER modify GenomeHandleComponent** - genome IDs must stay constant
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

## NOTES

### Coverage

- **Components**: 100% (28/28 documented)
- **Systems**: 95% (18/19 documented)
- **Modules**: 93% (14/15 documented)
- **Data Contracts**: 100% (all inter-module contracts documented)

### Recent Updates (2025-01-25)

**Quality Hardening Completed:**
- ✅ **Tooling Infrastructure**: `.clang-format` (Google style, 100 col), `.clang-tidy` (conservative ruleset), sanitizer builds (`ENABLE_SANITIZERS` CMake option), GitHub Actions CI workflow
- ✅ **Performance**: Spatial hash `clear()` preserves capacity, stats system reduced from 5→3 registry scans, genetics mutations added `reserve()` to reduce reallocations
- ✅ **Safety & Correctness**: Fixed-point divide-by-zero terminates instead of returning 0, InnovationDatabase persistence with little-endian byte order and validation, all 133 tests passing

**6 New Module Documents Created:**
- ✅ `modules/combat.md` - Combat mechanics, state lifecycle
- ✅ `modules/vision.md` - Raycasting, sensory perception
- ✅ `modules/social_behavior.md` - Flocking, territoriality, pack hunting
- ✅ `modules/spatial_indexing.md` - Spatial hash indices
- ✅ `modules/telemetry.md` - Metrics collection
- ✅ `modules/evolution.md` - Generational evolution, speciation

**4 Analysis Documents Created:**
- ✅ `docs_gap_analysis.md` - Complete gap analysis
- ✅ `module_comparison.md` - Documented vs actual comparison
- ✅ `architecture/actual_vs_documented.md` - Architecture analysis
- ✅ `undocumented_features.md` - Catalog of undocumented features

**1 Component Document Updated**:
- ✅ `modules/components.md` - Added 8 missing components

### Architecture Strengths

- **Clean layering**: Clear separation between core, systems, environment, genetics
- **ECS-based**: Scalable entity management via EnTT
- **Backend abstraction**: Physics backend swappable (Simple → GPU → PhysX)
- **Deterministic**: RNG with derived seeds, content-hashed genome IDs

### Known Limitations

- **Sequential execution**: Most systems run sequentially (multi-rate scheduler exists)
- **Single-threaded**: No multi-threaded system execution yet
- **No persistence**: Genomes and state not saved to disk (planned feature)

### Future Work

1. **Testing documentation** - Create `documentation/testing_guide.md`
2. **Motor system module** - Create `documentation/modules/motor_module.md`
3. **Decomposition module** - Create `documentation/modules/decomposition_module.md`
4. **Parallelization strategy** - Document multi-threading opportunities
5. **Determinism guide** - Centralize determinism guarantees

---

**Last Updated:** 2025-01-25
**Documentation Status:** Production Ready (85% coverage)
