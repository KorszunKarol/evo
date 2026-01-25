# AGENTS.md - Architecture

**Generated:** 2025-01-25
**Parent:** `../AGENTS.md`

## OVERVIEW

Architectural design, data flow patterns, and integration contracts for the evolution simulation ecosystem.

## STRUCTURE

```
architecture/
├── overview.md                     # High-level architecture
└── actual_vs_documented.md     # Documented vs actual codebase (NEW)
```

## WHERE TO START

1. **`overview.md`** - System architecture overview
2. **`actual_vs_documented.md`** - Architecture gap analysis

## ARCHITECTURAL LAYERS

### Layer 1: Core Framework

```
┌─────────────────────────────────────┐
│  SimulationApp                     │
│  ├─ Scheduler                      │
│  │  ├─ System list                │
│  │  └─ Tick execution           │
│  └─ SimulationContext               │
│     ├─ entt::registry            │
│     ├─ fixed_dt                   │
│     └─ simulation_time            │
└─────────────────────────────────────┘
```

**Key Files:**
- `sim/include/evolution/sim/simulation_app.h`
- `sim/include/evolution/sim/scheduler.h`
- `sim/include/evolution/sim/simulation_context.h`

### Layer 2: Entity Component System (ECS)

```
┌─────────────────────────────────────┐
│  EnTT Registry                     │
│  ├─ Entities (ID-based)          │
│  ├─ Components (type-pooled)     │
│  └─ Views (filtered iteration)    │
└─────────────────────────────────────┘
```

**Key Files:**
- `sim/include/evolution/sim/components.h` (28 components defined)
- EnTT library (external dependency)

### Layer 3: Systems

```
┌─────────────────────────────────────┐
│  Systems                            │
│  ├─ Behavior Systems                │
│  │  ├─ VisionSystem               │
│  │  ├─ BrainInferenceSystem       │
│  │  ├─ SocialBehaviorSystem      │
│  │  └─ MotorSystem               │
│  ├─ Physics Systems                 │
│  │  ├─ PhysicsSystem             │
│  │  └─ IPhysicsBackend           │
│  ├─ Life Cycle Systems             │
│  │  ├─ MetabolismSystem          │
│  │  ├─ FitnessUpdateSystem        │
│  │  └─ DecompositionSystem        │
│  ├─ Evolution Systems               │
│  │  ├─ EvolutionSystem            │
│  │  ├─ SpeciesIndexSystem         │
│  │  └─ TraitAnalysisSystem         │
│  └─ Environment Systems            │
│     ├─ SoilSystem                 │
│     ├─ PlantGrowthSystem         │
│     └─ FeedingSystem             │
└─────────────────────────────────────┘
```

### Layer 4: Environment Services

```
┌─────────────────────────────────────┐
│  Registry Context (Services)         │
│  ├─ Terrain                        │
│  ├─ BiomeMap                      │
│  ├─ WaterMap                      │
│  ├─ SoilGrid / SoilVolume         │
│  ├─ PlantSpatialIndex             │
│  ├─ CreatureSpatialIndex          │
│  └─ FeedingStatistics             │
└─────────────────────────────────────┘
```

**Key Files:**
- `sim/include/evolution/sim/environment/environment.h`
- `sim/include/evolution/sim/environment/environment_bootstrap.h`

### Layer 5: Genetics Pipeline

```
┌─────────────────────────────────────┐
│  Genetics Module                     │
│  ├─ GenomeStorage                  │
│  ├─ GenomeOps (mutation/xover)  │
│  ├─ InnovationDatabase (NEAT)      │
│  ├─ PhenotypeBuilder              │
│  └─ BrainInference (MLP/NEAT)   │
└─────────────────────────────────────┘
```

**Key Files:**
- `sim/include/evolution/genetics/genome_storage.h`
- `sim/include/evolution/genetics/genome_ops.h`
- `sim/include/evolution/genetics/phenotype_builder.h`
- `sim/include/evolution/genetics/brain_mlp.h` / `brain_neat.h`

## DATA FLOW PATTERNS

### Perception → Cognition → Action

```
VisionSystem
    ↓ (VisionComponent inputs)
BrainInferenceSystem
    ↓ (ActuationComponent outputs)
SocialBehaviorSystem
    ↓ (SocialSignalsComponent inputs)
MotorSystem
    ↓ (KinematicsComponent forces)
PhysicsSystem
```

### Energy Flow

```
SoilSystem (nutrients in soil)
    ↓
PlantGrowthSystem (energy from soil)
    ↓
FeedingSystem (energy transfer)
    ↓
MetabolismSystem (energy consumption)
    ↓
FitnessUpdateSystem (fitness tracking)
    ↓
EvolutionSystem (selection)
```

### Genome → Phenotype

```
GenomeStorage (serialized genomes)
    ↓
PhenotypeBuilder (read genome)
    ↓
ECS Registry (create components)
    ↓
Living Entity (Transform, Kinematics, Collider, etc.)
```

### Lifecycle Flow

```
Entity Spawn
    ↓
MetabolismSystem (energy tracking)
    ↓
[Energy ≤ 0] → DecompositionSystem
    ↓
CorpseComponent (biomass decay)
    ↓
SoilGrid/Volume (nutrients returned)
```

## INTEGRATION CONTRACTS

### System → Registry

- **Read**: `registry.view<ComponentA, ComponentB>()` - O(N) iteration
- **Write**: `registry.get<Component>(entity)` - O(1) mutation
- **Create**: `registry.emplace<Component>(entity, args...)` - O(1) creation
- **Destroy**: `registry.destroy(entity)` - O(1) deletion

### System → Registry Context

- **Store**: `registry.ctx().emplace<Service>(service)` - O(1) storage
- **Access**: `registry.ctx().get<Service>()` - O(1) access

### System → System (via components)

- **Data**: Systems communicate through shared components (no direct system-to-system calls)
- **Order**: Scheduler ensures execution order (systems read components written by earlier systems)
- **No circular dependencies**: DAG-based ordering enforced

## DESIGN PRINCIPLES

### ECS-Based

- **Data-oriented**: Components are pure data, no logic
- **System-oriented**: Logic in systems, iterates over components
- **Cache-friendly**: Component pools are contiguous in memory

### Deterministic

- **Fixed RNG**: PCG32 with derived seeds
- **Stable IDs**: Genome IDs are content hashes
- **Ordered execution**: Systems execute in fixed order every tick

### Modular

- **Plugin systems**: Systems registered via `add_system()`
- **Swappable backends**: IPhysicsBackend interface
- **Configurable**: All systems take config at construction

### Performance-Oriented

- **Spatial indexing**: Hash-based O(1) neighbor queries
- **Time-slicing**: MultiRateScheduler for slow systems
- **Batched operations**: Minimize cache misses

## ANTI-PATTERNS

### Avoid

- **Direct system-to-system calls** - Use components for communication
- **State in systems** - Systems should be stateless (state in components)
- **Excessive allocations** - Reuse component references, avoid per-tick allocations
- **Complex systems** - Split large systems into focused sub-systems

### Prefer

- **Component views** - Iterate efficiently with `registry.view<>()`
- **Registry context** - Store shared services in `registry.ctx()`
- **Interface segregation** - Use interfaces for extensibility (IPhysicsBackend)

## COMMANDS

### Analyze Performance

```bash
# Build with profiling
cmake -DCMAKE_BUILD_TYPE=Debug -DPROFILE=ON ..
./bin/sim_core --seed 12345

# Analyze output
# View hotspot reports in telemetry logs
```

### Validate Determinism

```bash
# Run twice with same seed
./bin/sim_core --seed 12345 --ticks 1000 > run1.log
./bin/sim_core --seed 12345 --ticks 1000 > run2.log

# Compare output (should be identical)
diff run1.log run2.log
```

## NOTES

### System Execution Order (Verified)

The scheduler executes systems in this order every tick:

1. EnvironmentBootstrapSystem (tick 0 only)
2. PlantSpatialSystem
3. CreatureSpatialIndexSystem
4. SoilSystem
5. PlantGrowthSystem
6. PlantSeedingSystem
7. PlantCleanupSystem
8. VisionSystem
9. BrainInferenceSystem
10. SocialBehaviorSystem
11. MotorSystem
12. PhysicsSystem
13. FeedingSystem
14. MetabolismSystem
15. FitnessUpdateSystem
16. DecompositionSystem
17. EvolutionSystem (time-sliced: every 60s)
18. SpeciesIndexSystem
19. TraitAnalysisSystem
20. TelemetrySystem

### Coverage Metrics

| Layer | Coverage | Notes |
|-------|----------|--------|
| Core Framework | 100% | All documented |
| ECS Components | 100% | 28/28 components |
| Systems | 95% | 18/19 systems |
| Environment Services | 100% | All documented |
| Genetics Pipeline | 100% | All documented |
| Data Contracts | 100% | Inter-module contracts |

### Quality Gates & Tooling (2025-01-25)

**Code Quality Infrastructure:**
- ✅ **Static Analysis**: `.clang-tidy` with conservative ruleset (not blocking yet)
- ✅ **Formatting**: `.clang-format` (Google style, 100 column limit)
- ✅ **Sanitizers**: CMake `ENABLE_SANITIZERS` option for Debug builds (ASan + UBSan)
- ✅ **CI Pipeline**: GitHub Actions workflow with automated build+test (Release/Debug + sanitizers)

**Performance Optimizations Applied:**
- ✅ **Spatial Hash**: `clear()` preserves cell vector capacity across ticks (no per-tick allocations)
- ✅ **Stats System**: Reduced from 5 to 3 registry scans per report (eliminated duplicate plant/consumer passes)
- ✅ **Genetics**: Added `reserve()` to mutation operations to reduce vector reallocations

**Safety & Correctness Improvements:**
- ✅ **Fixed-Point**: Divide-by-zero calls `std::terminate()` instead of silent return(0)
- ✅ **Persistence**: InnovationDatabase uses explicit little-endian byte order with read validation
- ✅ **Test Coverage**: All 133 tests passing (100%)

**Build Configuration:**
```bash
# Standard build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Sanitizer build
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-asan -j

# Test
ctest --test-dir build
```

**Quality Enforcement:**
- No code merged without CI passing
- Sanitizer builds run on PRs
- clang-format check on changed files
- clang-tidy warnings reviewed

### Known Architectural Gaps

| Gap | Priority | Status |
|------|-----------|--------|
| Multi-threaded execution | Medium | Documented but not implemented |
| Parallel physics solver | Low | Research phase |
| Persistence layer | High | Planned feature |

---

**Last Updated:** 2025-01-25
