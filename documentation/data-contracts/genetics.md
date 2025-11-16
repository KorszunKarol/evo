# Data Contracts: Genetics Module

## Overview

This document defines data contracts for the genetics module - how genomes flow between storage, phenotype building, brain inference, and reproduction systems.

## Contract Types

### 1. Genome Storage Contracts

Define how genomes are stored, retrieved, and identified.

### 2. Phenotype Building Contracts

Define how genomes are transformed into ECS entities.

### 3. Brain Inference Contracts

Define how brain genomes are evaluated and produce actuation commands.

### 4. Reproduction Contracts

Define how genomes are mutated, crossed over, and assigned to offspring.

---

## Genome Storage Contracts

### GenomeStorage ↔ Systems

**Read Contract**:
- **Readers**: PhenotypeBuilder, BrainInferenceSystem, ReproductionSystem
- **Method**: `storage.get(id)`
- **Returns**: `const Genome*` (immutable pointer)
- **Lifetime**: Valid until storage mutates (insert/erase)
- **Thread Safety**: Not thread-safe (storage must be synchronized externally)

**Write Contract**:
- **Writers**: GenomeStorage (via `create_random`, `insert`)
- **Method**: `storage.insert(genome_obj)` or `storage.create_random(seed)`
- **Returns**: `GenomeId` (content hash)
- **Side Effects**: Adds genome to storage, updates insertion order
- **Thread Safety**: Not thread-safe

**ID Contract**:
- **Computation**: Content hash (FNV-1a) of serialized buffer (excluding id field)
- **Stability**: Same content → same ID (deterministic)
- **Uniqueness**: Different content → different ID (with high probability)
- **Immutable**: ID computed from content, not assigned

**Data Format**:
```cpp
using GenomeId = std::uint64_t;

// Genome stored as FlatBuffers table
const evolution::genome::Genome* genome = storage.get(id);
```

**Guarantees**:
- Genome IDs are stable (content hash)
- Genome pointers remain valid until storage mutates
- Insertion preserves deterministic ordering

---

## Phenotype Building Contracts

### PhenotypeBuilder ↔ Registry

**Read Contract**:
- **Readers**: PhenotypeBuilder (reads genome from storage)
- **Read Fields**: Genome body, brain, metadata
- **Read Frequency**: Once per entity creation
- **Thread Safety**: Not thread-safe

**Write Contract**:
- **Writers**: PhenotypeBuilder
- **Write Components**: TransformComponent, KinematicsComponent, ColliderComponent, MetabolismComponent, BrainComponent, ActuationComponent, GenomeHandleComponent, ReproductionComponent
- **Write Frequency**: Once per entity creation
- **Thread Safety**: Not thread-safe
- **Validation**: Builder validates genome data, returns error on failure

**Component Creation Contract**:
1. **Required Components**: All components created atomically (or error returned)
2. **Existing Components**: Overridden with genome-driven values
3. **Determinism**: Same genome → same component values
4. **Validation**: Body size bounds, mass density, brain structure validated

**Data Flow**:
```
GenomeStorage::get(id) → const Genome*
    ↓
PhenotypeBuilder::build(id, registry, entity, storage)
    ↓
registry.emplace<TransformComponent>(entity, ...)
registry.emplace<KinematicsComponent>(entity, ...)
registry.emplace<ColliderComponent>(entity, ...)
... (all components)
```

**Guarantees**:
- All required components created (or error returned)
- Component values deterministic (same genome → same components)
- No partial updates (all-or-nothing)

---

## Brain Inference Contracts

### BrainInferenceSystem ↔ GenomeStorage

**Read Contract**:
- **Readers**: BrainInferenceSystem
- **Read Fields**: Genome brain definition (MLP or NEAT)
- **Read Frequency**: Every tick (when update interval elapses)
- **Thread Safety**: Not thread-safe

**Data Flow**:
```
BrainComponent.genome_id → GenomeId
    ↓
storage.get(genome_id) → const Genome*
    ↓
Read genome.brain_kind, genome.mlp or genome.neat
    ↓
Dispatch to BrainMlp::Evaluate() or BrainNeat::Evaluate()
```

**Guarantees**:
- Genome pointer valid during evaluation
- Storage not mutated during tick

### BrainInferenceSystem ↔ ActuationComponent

**Read Contract**:
- **Readers**: BrainInferenceSystem (reads BrainComponent for update rate)
- **Read Fields**: `BrainComponent.update_interval`, `BrainComponent.accum`
- **Read Frequency**: Every tick

**Write Contract**:
- **Writers**: BrainInferenceSystem
- **Write Fields**: `ActuationComponent.impulse_x`, `impulse_z`, `jump`, `eat`
- **Write Frequency**: When `accum >= update_interval`
- **Thread Safety**: Not thread-safe

**Update Rate Contract**:
1. **Accumulation**: `BrainComponent.accum += dt` each tick
2. **Evaluation**: When `accum >= update_interval`:
   - Execute brain inference
   - Write outputs to `ActuationComponent`
   - Reset `accum = 0.0`
3. **Skipping**: When `accum < update_interval`, increment `update_skip` counter

**Output Mapping Contract**:
- `outputs[0]` → `ActuationComponent.impulse_x` (clamped `[-1, 1]`)
- `outputs[1]` → `ActuationComponent.impulse_z` (clamped `[-1, 1]`)
- `outputs[2]` → `ActuationComponent.jump` (threshold `> 0.5`)
- `outputs[3]` → `ActuationComponent.eat` (threshold `> 0.5`)

**Guarantees**:
- Outputs clamped to valid ranges
- Commands reset after `MotorSystem` consumes them

### BrainInferenceSystem ↔ MotorSystem

**Execution Order Contract**:
1. `BrainInferenceSystem::tick()` - Writes actuation commands
2. `MotorSystem::tick()` - Consumes commands, applies forces

**Data Flow**:
```
BrainInferenceSystem::tick()
    ↓
ActuationComponent.impulse_x, impulse_z, jump, eat
    ↓
MotorSystem::tick()
    ↓
KinematicsComponent.accumulated_force += forces
MetabolismComponent.energy -= movement_cost
    ↓
ActuationComponent reset to defaults
```

**Guarantees**:
- Actuation commands available for motor system
- Commands reset after motor consumption

---

## Reproduction Contracts

### ReproductionSystem ↔ GenomeStorage

**Read Contract**:
- **Readers**: ReproductionSystem
- **Read Fields**: Parent genomes (via `GenomeHandleComponent.id`)
- **Read Frequency**: When reproduction conditions met (energy threshold, cooldown)

**Write Contract**:
- **Writers**: ReproductionSystem
- **Write Fields**: New offspring genome (via `storage.insert()`)
- **Write Frequency**: When reproduction occurs
- **Thread Safety**: Not thread-safe

**Reproduction Flow Contract**:
1. **Selection**: Find mate within `mate_radius` (same species preferred)
2. **Crossover**: `GenomeOps::crossover(parent_a, parent_b, config, seed)`
3. **Mutation**: `GenomeOps::mutate(offspring, config, seed)`
4. **Insertion**: `storage.insert(offspring)` → `GenomeId`
5. **Spawn**: `PhenotypeBuilder::build(offspring_id, registry, entity, storage)`
6. **Energy Split**: Both parents lose energy (reproduction cost)

**Determinism Contract**:
- All stochastic operations use derived seeds: `(global_seed ^ genome_seed ^ op_tag ^ counter)`
- Same parents + seed → same offspring (deterministic)
- Seed derivation ensures no collisions across operations

**Guarantees**:
- Offspring genomes valid (pass validation)
- Offspring inserted into storage before spawn
- Energy costs deducted from parents

### GenomeOps ↔ Reproduction

**Mutation Contract**:
- **Input**: Source genome, mutation config, deterministic seed
- **Output**: Mutated genome copy
- **Determinism**: Same genome + seed → same mutation
- **Validation**: Mutated values clamped to valid ranges

**Crossover Contract**:
- **Input**: Two parent genomes, crossover config, deterministic seed
- **Output**: Offspring genome
- **MLP**: Layer-wise blend (random parent weights, averaged biases)
- **NEAT**: Innovation-based alignment (match genes → choose/blend, include disjoint/excess)
- **Determinism**: Same parents + seed → same offspring

**Compatibility Distance Contract**:
- **Input**: Two genomes
- **Output**: `double` distance metric
- **Formula**: `δ = c1*Excess + c2*Disjoint + c3*AvgWeightDiff + c4*BodyDiff`
- **Properties**: `δ(a, a) = 0`, `δ(a, b) = δ(b, a)` (symmetric)

---

## Species Indexing Contracts

### SpeciesIndexSystem ↔ GenomeStorage

**Read Contract**:
- **Readers**: SpeciesIndexSystem
- **Read Fields**: All genomes in storage (via `storage.ids()`)
- **Read Frequency**: Every generation (or on-demand)

**Write Contract**:
- **Writers**: SpeciesIndexSystem
- **Write Fields**: Species assignments (per-genome `SpeciesId`)
- **Write Frequency**: When clustering updates

**Clustering Contract**:
1. **Distance Computation**: `GenomeOps::compatibility_distance(a, b)` for all pairs
2. **Clustering**: Threshold-based or k-means assignment
3. **Assignment**: Each genome assigned to `SpeciesId`
4. **Telemetry**: Species count, mean δ, mean brain edges logged

**Guarantees**:
- Each genome assigned to exactly one species
- Species assignments deterministic (same genomes → same species)

---

## Contract Violations

### Undefined Behavior

- Accessing genome that doesn't exist (`storage.get(invalid_id)` → `nullptr`)
- Modifying storage from multiple threads without synchronization
- Building phenotype with invalid entity (not owned by registry)
- Evaluating brain with mismatched input/output sizes

### Precondition Violations

- `GenomeId == 0` (invalid/uninitialized)
- Body size out of bounds `[0.1, 10.0]` meters
- Mass density out of bounds `[100.0, 5000.0]` kg/m³
- Brain weights contain NaN/Inf

### Postcondition Guarantees

- Genome IDs are stable (content hash)
- Phenotype components deterministic (same genome → same components)
- Brain outputs clamped to `[-1, 1]`
- Offspring genomes valid (pass validation)

---

## Future Contracts

### Persistence Contract (Planned)

- **Save**: `storage.save_all()` → disk snapshot
- **Load**: `storage.load_from_disk(path)` → restore genomes
- **Format**: FlatBuffers binary files with versioning

### GPU Inference Contract (Planned)

- **Data Transfer**: Host → Device before kernel launch
- **Kernel Execution**: Batch processing on GPU
- **Data Transfer**: Device → Host after kernel completion
- **Synchronization**: CUDA streams for async execution

---

## Related Documentation

- [Genome Module](../modules/genome.md) - Genome storage and operations
- [Phenotype Module](../modules/phenotype.md) - Building ECS entities
- [Brain Module](../modules/brain.md) - MLP and NEAT inference
- [Core Simulation Module](../modules/core_simulation.md) - System execution order

