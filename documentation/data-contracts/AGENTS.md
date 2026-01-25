# AGENTS.md - Data Contracts

**Generated:** 2025-01-25
**Parent:** `../AGENTS.md`

## OVERVIEW

Inter-module data contracts defining read/write semantics, data flow, and guarantees between simulation systems.

## STRUCTURE

```
data-contracts/
├── inter_module_contracts.md    # All system-to-system contracts
└── genetics.md                  # Genetics module contracts
```

## WHERE TO START

1. **`inter_module_contracts.md`** - System-level contracts
2. **`genetics.md`** - Genome storage and evolution contracts

## CONTRACT CATEGORIES

### Genome Storage Contracts

**Contract:** GenomeStorage ↔ All Systems

**Readers:**
- PhenotypeBuilder - Read genomes for entity creation
- BrainInferenceSystem - Read brain definitions (MLP/NEAT)
- ReproductionSystem - Read parent genomes for crossover
- EvolutionSystem - Read all genomes for selection

**Writers:**
- GenomeStorage - Insert new genomes (random or offspring)
- No system writes to existing genomes (immutable once inserted)

**Guarantees:**
- Genome IDs are stable (content hash)
- Genome pointers valid until storage mutates
- Deterministic insertion ordering

### Phenotype Building Contracts

**Contract:** PhenotypeBuilder ↔ ECS Registry

**Readers:**
- PhenotypeBuilder - Read genome from GenomeStorage

**Writers:**
- PhenotypeBuilder - Create/overwrite components on entity

**Guarantees:**
- All required components created (or error returned)
- Component values deterministic (same genome → same components)
- TransformComponent preserved if pre-existing

### Brain Inference Contracts

**Contract:** BrainInferenceSystem ↔ ActuationComponent

**Readers:**
- BrainInferenceSystem - Read ActuationComponent for update rate

**Writers:**
- BrainInferenceSystem - Write motor commands (impulse_x, impulse_z, jump, eat)

**Guarantees:**
- Outputs clamped to valid ranges `[-1, 1]`
- Commands reset after MotorSystem consumes them
- Update interval respected (brain runs at configured Hz)

### Vision System Contracts

**Contract:** VisionSystem ↔ IPhysicsBackend

**Readers:**
- VisionSystem - Read VisionComponent configuration

**Writers:**
- VisionSystem - Write ray hit results (distances, types, entities)

**Guarantees:**
- Ray distances accurate (from physics backend)
- Hit types classified correctly
- Out-of-range hits return null entity

### Social Behavior Contracts

**Contract:** SocialBehaviorSystem ↔ Multiple Components

**Readers:**
- SocialBehaviorSystem - Read BrainComponent, VisionComponent, TerritoryComponent

**Writers:**
- SocialBehaviorSystem - Write SocialSignalsComponent, CombatComponent

**Guarantees:**
- Social signals computed from actual neighbor state
- Combat state transitions follow game rules
- Territory updated only when creature moves

### Environment Service Contracts

**Contract:** All Systems ↔ Environment Services (Registry Context)

**Readers:**
- All systems - Read Terrain, BiomeMap, WaterMap, SoilGrid/Volume

**Writers:**
- No systems write to environment services (read-only after bootstrap)

**Guarantees:**
- Environment services initialized before first tick
- Services persist for simulation lifetime
- Biome/Water maps derived deterministically from terrain

### Evolution System Contracts

**Contract:** EvolutionSystem ↔ Multiple Systems

**Readers:**
- EvolutionSystem - Read FitnessComponent, GenomeHandleComponent
- EvolutionSystem - Read GenomeStorage for genome data

**Writers:**
- EvolutionSystem - Create new genomes (via GenomeOps::crossover/mutate)
- EvolutionSystem - Insert genomes into storage
- EvolutionSystem - Trigger phenotype building for offspring

**Guarantees:**
- Offspring genomes valid (pass validation)
- Selection deterministic (same population → same survivors)
- Generation intervals respected

### Decomposition Contracts

**Contract:** DecompositionSystem ↔ Soil Services

**Readers:**
- DecompositionSystem - Read CorpseComponent

**Writers:**
- DecompositionSystem - Add nutrients to SoilGrid/Volume

**Guarantees:**
- Biomass decay rate respects environment
- Nutrients added to correct voxel
- Corpse destroyed when biomass ≤ 0

## DATA FLOW DIAGRAMS

### Genome → Entity Flow

```
GenomeStorage::get(genome_id)
    ↓
PhenotypeBuilder::build(genome_id, registry, entity, storage)
    ↓
ECS Registry (create components)
    ↓
TransformComponent
KinematicsComponent
ColliderComponent
MetabolismComponent
BrainComponent
ActuationComponent
GenomeHandleComponent
ReproductionComponent
...
```

### Perception → Action Flow

```
VisionSystem::tick()
    ↓ (populate VisionComponent)
BrainInferenceSystem::tick()
    ↓ (read Brain, Vision; write Actuation)
ActuationComponent (motor commands)
    ↓
MotorSystem::tick()
    ↓ (read Actuation; write Kinematics)
KinematicsComponent (forces, velocity)
    ↓
PhysicsSystem::step()
    ↓ (apply physics integration)
TransformComponent (position, rotation)
```

### Energy Cycle Flow

```
SoilGrid/Volume (nutrients)
    ↓
PlantGrowthSystem::tick()
    ↓ (consume nutrients)
PlantComponent.energy
    ↓
FeedingSystem::tick()
    ↓ (transfer energy)
Herbivore MetabolismComponent.energy
    ↓
MetabolismSystem::tick()
    ↓ (basal consumption)
MetabolismComponent.energy decreased
    ↓
[Energy ≤ 0] → CorpseComponent created
    ↓
DecompositionSystem::tick()
    ↓
SoilGrid/Volume (nutrients returned)
```

### Evolution Cycle Flow

```
MetabolismSystem::tick()
    ↓ (age, energy tracking)
FitnessComponent
    ↓
FitnessUpdateSystem::tick()
    ↓ (fitness calculation)
FitnessComponent.last_fitness
    ↓
[Generation interval elapsed] → EvolutionSystem::tick()
    ↓ (selection, crossover, mutation)
GenomeStorage (new offspring genomes)
    ↓
PhenotypeBuilder::build(offspring_id, ...)
    ↓
New entities spawned
```

## VIOLATION CONSEQUENCES

### Undefined Behavior

Accessing genome that doesn't exist:
- `storage.get(invalid_id)` → `nullptr`
- **Consequence:** Null pointer dereference (crash)
- **Fix:** Check genome existence before access

Writing to GenomeStorage from multiple threads:
- **Consequence:** Race conditions, data corruption
- **Fix:** Serialize access or use thread-safe wrapper

Building phenotype with invalid genome:
- **Consequence:** Partial component creation, entity in invalid state
- **Fix:** Validate genome before PhenotypeBuilder::build()

### Precondition Violations

Brain inputs/outputs mismatch:
- **Consequence:** Array bounds violation, incorrect behavior
- **Fix:** Validate brain structure at genome creation

Negative energy after reproduction cost:
- **Consequence:** Immediate starvation, entity death
- **Fix:** Check energy threshold before reproduction

## THREAD SAFETY

### Thread-Safe Contracts

- **GenomeStorage**: NOT thread-safe (must synchronize externally)
- **ECS Registry**: NOT thread-safe (must synchronize externally)
- **Environment Services**: NOT thread-safe (read-only after bootstrap)
- **Spatial Indices**: NOT thread-safe (must synchronize during rebuild)

### Synchronization Patterns

```cpp
// For thread-safe genome storage access
std::lock_guard<std::mutex> lock(storage_mutex_);
auto genome = storage.get(genome_id);

// For parallel system execution (if implemented)
std::unique_lock lock(system_mutex_);
system.tick(context);
```

## PERFORMANCE GUARANTEES

### Complexity Guarantees

| Operation | Complexity | Notes |
|-----------|-------------|--------|
| Genome lookup | O(1) | Hash table access |
| Phenotype build | O(C + W) | C = components (~8), W = weights |
| Vision raycast | O(R × E) | R = rays, E = entity candidates |
| Spatial query | O(C + R²) | C = cells in radius, R = entities per cell |
| Fitness eval | O(P) | P = population size |

### Memory Guarantees

| Data Structure | Memory | Notes |
|---------------|--------|--------|
| EnTT Registry | O(E × C_avg) | E = entities, C_avg = components per entity |
| Genome Storage | O(G × S) | G = genome size, S = genome count |
| Spatial Index | O(W×H) | W, H = grid dimensions |
| Vision Results | O(R × E) | R = ray count, E = entities |

## VALIDATION RULES

### Genome Validation

- Body size bounds: `[0.1, 10.0]` meters
- Mass density: `[100.0, 5000.0]` kg/m³
- Brain weights: No NaN/Inf

### Component Validation

- Transform position: Within world bounds
- Energy values: `≥ 0` and `≤ max_energy`
- Genome IDs: Non-zero (invalid reserved)

### System Contracts

- Systems MUST validate inputs before use
- Systems MUST handle missing components gracefully
- Systems MUST report errors via logging (not exceptions for expected failures)

## COMMANDS

### Verify Contract Compliance

```bash
# Run simulation with contract validation enabled
./bin/sim_core --validate-contracts --seed 12345

# Check logs for contract violations
grep "CONTRACT_VIOLATION" logs/simulation.log
```

## NOTES

### Contract Evolution

- **Initial**: Minimal contracts, direct component access
- **Current**: Comprehensive contracts, spatial indexing, deterministic guarantees
- **Future**: Transaction support, versioned contracts for hot-reload

### Missing Contracts

| System | Missing Contract | Priority |
|--------|-----------------|----------|
| MotorSystem | Motor→Physics force mapping | Low |
| CombatResolutionSystem | Damage application, health management | Medium (not implemented) |

---

**Last Updated:** 2025-01-25
