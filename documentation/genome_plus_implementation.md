# Genome++ Implementation Summary

## Overview

This document summarizes the implementation of the Genome++ features: preference-driven mate selection, species indexing, and enhanced compatibility distance/crossover/mutation logic.

## Implemented Features

### 1. Trait Extraction (`sim/include/evolution/genetics/trait_extraction.h`)

- **ExtractTraitVector()**: Extracts normalized 8-dimensional trait vectors from genomes
  - Traits: size_x, size_y, size_z, mass_density, color_r, color_g, color_b, brain_size
  - All values normalized to [0, 1] range
  - Deterministic: same genome → same traits

- **TraitCosineDistance()**: Computes cosine distance between trait vectors
  - Returns distance in [0, 2] range (0 = identical, 2 = opposite)
  - Used in compatibility distance computation

### 2. Enhanced Compatibility Distance (`sim/src/genetics/genome_ops.cpp`)

The `compatibility_distance()` function now computes a rich distance metric:

- **Body Distance**: Normalized L2 on size (x,y,z), mass_density, shape penalty
- **Brain MLP Distance**: Layer-wise mean absolute weight difference (normalized by layer size)
- **Brain NEAT Distance**: Classic NEAT metric:
  - Excess genes (beyond max innovation)
  - Disjoint genes (different innovations)
  - Average weight difference (matching innovations)
- **Trait Latent Cosine Distance**: Cosine distance on trait embeddings
- **Combined**: `δ = w_body × body_dist + w_brain × brain_dist + w_trait × trait_dist`

Properties:
- `δ(a, a) = 0` (identical genomes)
- `δ(a, b) = δ(b, a)` (symmetric)
- `δ ≥ 0` (non-negative)
- Large penalty (2.0) for different brain kinds

### 3. Enhanced Mutation (`sim/src/genetics/genome_ops.cpp`)

Mutation now supports:

- **Mutator Gene**: Genome-coded mutation rates override config defaults
  - `mutate_rate_struct`: Structural mutation probability
  - `mutate_rate_param`: Parameter mutation probability
  - `weight_sigma`: Gaussian σ for weight mutations

- **Body Mutations**:
  - Mass density jitter
  - Shape flips (Sphere ↔ CapsuleY ↔ Box)
  - Color jitter

- **Brain Mutations**:
  - MLP: Weight/biases jitter, update_rate perturbation
  - NEAT: Weight perturbation, connection toggle (enable/disable)

- **Module Mutations**:
  - Rare module duplication (structural change)

- **Gating/Preference Mutations**:
  - Weight and bias jitter for gating networks
  - Weight and bias jitter for preference networks

### 4. Enhanced Crossover (`sim/src/genetics/genome_ops.cpp`)

Crossover now supports:

- **Module-Level Selection**: Per-module parent choice (weighted by fitness proxy)
- **MLP Blending**: Random parent per weight, averaged biases
- **Gating Blending**: Random parent per weight, averaged biases
- **Preference Blending**: Random parent per weight, averaged biases
- **Hotspot Merging**: Combines recombination hotspots from both parents (up to 5)
- **Mutator Inheritance**: Inherits mutator gene from fitter parent
- **Life Stage Inheritance**: Inherits life stages from fitter parent

### 5. Reproduction System (`sim/include/evolution/sim/reproduction_system.h`)

**Features**:
- Preference-driven mate selection using PreferenceNet
- Fallback to trait similarity when PreferenceNet absent
- Distance and energy weighting for acceptance probability
- Asexual fallback option (configurable)
- Deterministic seed derivation for all operations

**Flow**:
1. Find eligible parents (cooldown expired, energy threshold met)
2. Find candidate mates within `mate_radius`
3. Evaluate preference for each candidate (PreferenceNet or trait similarity)
4. Sort by acceptance probability
5. Attempt reproduction with highest-probability candidates
6. Crossover + mutate offspring
7. Spawn offspring entity
8. Deduct energy costs from parents

### 6. Species Index System (`sim/include/evolution/sim/species_index_system.h`)

**Features**:
- Threshold-based clustering using compatibility distance
- Dynamic threshold adjustment to maintain target species count
- Species assignment per genome (stored in `species_map_`)
- Telemetry: species count, size distribution

**Algorithm**:
1. For each genome, find closest existing species (within threshold)
2. Assign to existing species or create new one
3. Adjust threshold based on current species count:
   - Too few species → decrease threshold (tighter clustering)
   - Too many species → increase threshold (looser clustering)

## Tests

### Test Files Created

1. **`tests/genetics/test_trait_extraction.cpp`**:
   - Trait vector extraction
   - Cosine distance computation
   - Determinism checks

2. **`tests/genetics/test_compatibility_distance.cpp`**:
   - Identical genome distance (should be 0)
   - Symmetry property
   - Non-negativity
   - Different brain kind penalty

3. **`tests/genetics/test_mutation_crossover.cpp`**:
   - Mutation determinism (same seed → same mutation)
   - Crossover determinism (same parents + seed → same child)
   - Parent ID tracking in offspring
   - Mutator gene preservation

## Integration Points

### Systems Integration

- **ReproductionSystem**: Should run after MetabolismSystem (to ensure energy thresholds accurate)
- **SpeciesIndexSystem**: Can run periodically (expensive O(N²) operation)
- **BrainInferenceSystem**: Already updated to support gating and modules

### Component Updates

- **ReproductionComponent**: Existing component used as-is
- **GenomeHandleComponent**: Existing component used as-is
- **LifecycleComponent**: New component added for life stage tracking

## Performance Considerations

- **Compatibility Distance**: O(N + M) where N/M = edge counts (NEAT) or weight counts (MLP)
- **Reproduction System**: O(N × M) where N = eligible parents, M = candidates in radius
- **Species Indexing**: O(N²) for distance matrix computation (consider running periodically)

## Future Enhancements

- NEAT innovation alignment in crossover (currently simplified)
- Batch module evaluation optimization
- GPU-accelerated preference evaluation
- Species-based fitness sharing
- Plant toxicity/detox tolerance hooks

## Files Modified/Created

### New Files
- `sim/include/evolution/genetics/trait_extraction.h`
- `sim/src/genetics/trait_extraction.cpp`
- `sim/include/evolution/sim/reproduction_system.h`
- `sim/src/reproduction_system.cpp`
- `sim/include/evolution/sim/species_index_system.h`
- `sim/src/species_index_system.cpp`
- `tests/genetics/test_trait_extraction.cpp`
- `tests/genetics/test_compatibility_distance.cpp`
- `tests/genetics/test_mutation_crossover.cpp`

### Modified Files
- `proto/genome.fbs` (schema extensions)
- `sim/include/evolution/genetics/genome_types.h` (DerivedTraits extensions)
- `sim/src/genetics/genome_ops.cpp` (enhanced distance/mutation/crossover)
- `sim/src/genetics/genome_storage.cpp` (mutator/hotspot/life stage seeding)
- `sim/src/genetics/derived_traits.cpp` (module/trait latent computation)
- `sim/src/genetics/phenotype_builder.cpp` (lifecycle component creation)
- `sim/include/evolution/genetics/rng.h` (derive_seed function)
- `sim/src/genetics/rng.cpp` (derive_seed implementation)
- `sim/src/brain_inference_system.cpp` (gating and module execution)
- `sim/include/evolution/sim/components.h` (LifecycleComponent)
- `CMakeLists.txt` (added test files)

## Usage Example

```cpp
// Setup
GenomeStorage storage;
ReproConfig config{};
ReproductionSystem repro_system(storage, config, 12345);
SpeciesIndexSystem species_system(storage, config, 10, 3.0);

// Register systems
app.scheduler().add_system(std::make_unique<ReproductionSystem>(storage, config, 12345));
app.scheduler().add_system(std::make_unique<SpeciesIndexSystem>(storage, config, 10, 3.0));

// Systems will automatically:
// - Find mates using preference networks
// - Cluster genomes into species
// - Maintain species assignments
```

## Determinism Guarantees

All operations use deterministic seed derivation:
- `derive_seed(global_seed, genome_id, op_tag, counter)`
- Same inputs → same outputs
- Seed collisions avoided via bit mixing

## Backward Compatibility

- Old genomes (without modules/gating/preference) work with default single-module behavior
- Phenotype builder handles missing fields gracefully
- Systems fall back to simple rules when preference networks absent

