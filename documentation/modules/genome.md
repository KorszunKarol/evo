# Module: Genome System

## Overview

The genome system provides deterministic storage, manipulation, and serialization of genetic data using FlatBuffers. It supports both MLP and NEAT brain architectures, body morphology encoding, and deterministic reproduction operations (mutation, crossover, speciation).

## Files

- `sim/include/evolution/genetics/genome_storage.h`
- `sim/src/genetics/genome_storage.cpp`
- `sim/include/evolution/genetics/genome_ops.h`
- `sim/src/genetics/genome_ops.cpp`
- `sim/include/evolution/genetics/genome_types.h`
- `sim/include/evolution/genetics/rng.h`
- `sim/src/genetics/rng.cpp`
- `proto/genome.fbs` (FlatBuffers schema)

## Schema (FlatBuffers)

The genome schema (`proto/genome.fbs`) defines versioned serialization format:

- **Body**: Shape type (Sphere/CapsuleY/Box), size parameters, mass density, color
- **Brain**: Either MLP (multilayer perceptron) or NEAT (neuroevolution of augmenting topologies)
- **Metadata**: Version, content hash ID, generation, parent IDs, RNG seed
- **Cached Traits**: Derived properties (mass, basal_rate, brain_cost)

## Key Types

### GenomeId

```cpp
using GenomeId = std::uint64_t;
```

**Purpose**: Stable identifier for genomes, computed as content hash.

**Properties**:
- Deterministic: Same genome content → same ID
- Unique: Different content → different ID (with high probability)
- Immutable: ID computed from serialized buffer (excluding id field)

### GenomeStorage

**Purpose**: In-memory storage and creation of serialized genomes.

**Responsibilities**:
- Store serialized genome buffers
- Generate random seed genomes
- Compute stable IDs via content hashing
- Provide lookup by ID

**Public API**:

```cpp
class GenomeStorage {
public:
    GenomeStorage() = default;
    GenomeId create_random(std::uint64_t seed);
    GenomeId insert(evolution::genome::GenomeT genome_obj);
    const evolution::genome::Genome* get(GenomeId id) const noexcept;
    bool contains(GenomeId id) const noexcept;
    std::vector<GenomeId> ids() const;
    void save_all() const noexcept;  // Stub for future persistence
};
```

**Parameters**:

- `create_random(seed)`:
  - **Input**: Deterministic seed for RNG
  - **Returns**: `GenomeId` assigned to created genome
  - **Complexity**: O(M) where M = serialized genome size
  - **Note**: Generates both body and brain with bounded parameter ranges

- `insert(genome_obj)`:
  - **Input**: Object-based genome representation
  - **Returns**: `GenomeId` (recomputed if id was zero)
  - **Complexity**: O(M) serialization + O(log N) hash map insertion
  - **Note**: Reuses existing genomes when identical hash present

- `get(id)`:
  - **Input**: Genome identifier
  - **Returns**: Pointer to immutable `Genome` table, or `nullptr` if missing
  - **Complexity**: O(1) average hash lookup
  - **Lifetime**: Pointer valid until storage mutates

**State Management**:

- `genomes_`: `std::unordered_map<GenomeId, Buffer>` - Hash map of serialized buffers
- `insertion_order_`: `std::vector<GenomeId>` - Deterministic ordering for telemetry

**Thread Safety**: Not thread-safe; synchronize externally if accessed from multiple threads.

**Performance**:
- Storage: O(N × M) where N = genome count, M = average buffer size
- Lookup: O(1) average, O(N) worst-case
- Insertion: O(M) serialization + O(log N) hash map operation

### GenomeOps

**Purpose**: Deterministic mutation, crossover, and speciation distance calculations.

**Responsibilities**:
- Mutate genomes (body, MLP, NEAT)
- Crossover genomes (MLP layer-wise, NEAT innovation-aligned)
- Compute compatibility distance for speciation

**Public API**:

```cpp
class GenomeOps {
public:
    static Genome mutate(const Genome& g, const ReproConfig& config, std::uint64_t seed);
    static Genome crossover(const Genome& a, const Genome& b, const ReproConfig& config, std::uint64_t seed);
    static double compatibility_distance(const Genome& a, const Genome& b);
};
```

**Parameters**:

- `mutate(genome, config, seed)`:
  - **Input**: Source genome, mutation configuration, deterministic seed
  - **Returns**: Mutated genome copy
  - **Complexity**: O(M) where M = genome size
  - **Mutations**:
    - Body: Size jitter (lognormal/Gaussian), mass density clamp, color jitter, shape flips
    - MLP: Add/remove hidden layer (rare), weight/biases jitter (Gaussian σ), update_rate jitter
    - NEAT: Add-connection, add-node (split), toggle enable, weight perturb, recurrent edges

- `crossover(parent_a, parent_b, config, seed)`:
  - **Input**: Two parent genomes, reproduction config, deterministic seed
  - **Returns**: Offspring genome
  - **Complexity**: O(M) where M = larger parent size
  - **MLP**: Layer-wise blend (random parent weights, averaged biases)
  - **NEAT**: Innovation-based alignment (match genes → choose/blend, include disjoint/excess from fittest)

- `compatibility_distance(genome_a, genome_b)`:
  - **Input**: Two genomes to compare
  - **Returns**: `double` distance metric
  - **Complexity**: O(N + M) where N, M = gene counts
  - **Formula**: `δ = c1*Excess + c2*Disjoint + c3*AvgWeightDiff + c4*BodyDiff`
  - **Note**: Used for speciation clustering

**Thread Safety**: Static functions are reentrant; no shared state.

### ReproConfig

**Purpose**: Configuration structure for reproduction operations.

**Structure**:

```cpp
struct ReproConfig {
    double mutate_rate_struct{0.05};   // Structural mutation probability
    double mutate_rate_param{0.2};     // Parameter mutation probability
    double weight_sigma{0.2};          // Gaussian σ for weight mutations
    // NEAT-specific parameters (excess/disjoint weights, add-node/add-conn rates, etc.)
};
```

### RNG Utilities

**Purpose**: Deterministic random number generation for genome operations.

**Files**:
- `sim/include/evolution/genetics/rng.h`
- `sim/src/genetics/rng.cpp`

**Key Functions**:

```cpp
namespace evolution::genetics {

// PCG-based deterministic RNG
class DeterministicRng {
public:
    explicit DeterministicRng(std::uint64_t seed) noexcept;
    std::uint64_t next() noexcept;
    double uniform(double min, double max) noexcept;
    double normal(double mean, double sigma) noexcept;
    double lognormal(double mean, double sigma) noexcept;
};

// Hash-based seed derivation
std::uint64_t derive_seed(std::uint64_t global_seed,
                          std::uint64_t genome_seed,
                          std::uint32_t op_tag,
                          std::uint32_t counter) noexcept;

}
```

**Determinism Guarantees**:
- Same inputs → same outputs
- Seed derivation: `(global_seed ^ genome_seed ^ op_tag ^ counter)` → PCG state
- All stochastic operations use derived seeds

## Data Contracts

### GenomeStorage ↔ Systems

**Contract**: Systems access genomes via `GenomeStorage::get()`.

**Data Flow**:
- Read: `storage.get(id)` → `const Genome*` (immutable)
- Write: `storage.insert(genome_obj)` → `GenomeId` (ownership transfer)

**Guarantees**:
- Genome pointers remain valid until storage mutates
- IDs are stable (content hash)
- Insertion preserves deterministic ordering

### GenomeOps ↔ Reproduction

**Contract**: Reproduction systems call `mutate()` and `crossover()`.

**Data Flow**:
- Input: Parent genomes (from storage)
- Output: Offspring genome (inserted into storage)

**Guarantees**:
- Deterministic: Same parents + seed → same offspring
- Valid: Offspring genomes pass validation (clamps, bounds)

## Dependencies

### Internal Dependencies

- `sim/components`: Component definitions used by phenotype builder
- `sim/math_types`: Vec3 for body size parameters

### External Dependencies

- **FlatBuffers**: Serialization framework
- **EnTT**: ECS registry (for phenotype building)

## Extension Points

### Adding New Body Shapes

1. Extend `ShapeType` enum in `genome.fbs`
2. Add shape payload struct
3. Update `PhenotypeBuilder` to handle new shape
4. Update mutation logic in `GenomeOps`

### Adding New Brain Types

1. Extend `BrainKind` enum in `genome.fbs`
2. Add brain table definition
3. Implement inference engine (like `BrainMlp`, `BrainNeat`)
4. Update `BrainInferenceSystem` to dispatch to new engine

### Custom Mutation Strategies

1. Extend `ReproConfig` with new parameters
2. Implement mutation logic in `GenomeOps::mutate()`
3. Ensure determinism via RNG seed derivation

## Performance Considerations

- **Storage**: Hash map provides O(1) average lookup
- **Serialization**: FlatBuffers zero-copy reads (no parsing overhead)
- **Mutation/Crossover**: O(M) where M = genome size
- **Memory**: O(N × M) where N = genome count, M = average buffer size

## Related Documentation

- [Phenotype Module](./phenotype.md) - Building ECS entities from genomes
- [Brain Module](./brain.md) - MLP and NEAT inference engines
- [Data Contracts](../data-contracts/genetics.md) - Inter-module data flow

