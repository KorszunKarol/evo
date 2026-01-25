# Module: Evolution

## Overview

The Evolution module manages generational evolution, selection pressure, mutation, crossover, and population dynamics. It implements evolutionary algorithms to improve creature fitness over time.

## Files

- `/home/karolito/evolution/sim/include/evolution/sim/evolution_system.h`
- `/home/karolito/evolution/sim/src/systems/evolution_system.cpp`
- `/home/karolito/evolution/sim/include/evolution/sim/systems/speciation_system.h`
- `/home/karolito/evolution/sim/src/systems/speciation_system.cpp`
- `/home/karolito/evolution/sim/include/evolution/sim/systems/trait_analysis_system.h`
- `/home/karolito/evolution/sim/src/systems/trait_analysis_system.cpp`
- `/home/karolito/evolution/sim/include/evolution/sim/species_index_system.h`
- `/home/karolito/evolution/sim/src/systems/species_index_system.cpp`

## Component Definitions

### FitnessComponent

**Purpose**: Tracks evolutionary fitness metrics for an entity.

**Fields**:
- `age_seconds` (double) - Total age in simulation time
- `energy_int_accum` (double) - Integrated energy over lifetime (area under energy curve)
- `offspring_count` (uint32_t) - Number of successful reproductions
- `last_fitness` (double) - Most recent computed fitness score

**Data Contract**:
- **Readers**: FitnessUpdateSystem, EvolutionSystem
- **Writers**: FitnessUpdateSystem
- **Lifetime**: Updated continuously while entity alive

## Systems

### EvolutionSystem

**Purpose**: Orchestrates generational evolution: selection, reproduction, mutation, species tracking.

**Algorithm**:

#### 1. Generation Trigger

```cpp
void tick(SimulationContext& context) {
    generation_timer_ += context.fixed_dt;

    if (generation_timer_ >= config_.generation_interval) {
        run_generation(context.registry);
        generation_timer_ = 0.0;
    }
}
```

#### 2. Fitness Evaluation

```cpp
for (auto entity : registry.view<FitnessComponent, MetabolismComponent>()) {
    auto& fitness = registry.get<FitnessComponent>(entity);
    auto& metabolism = registry.get<MetabolismComponent>(entity);

    // Age fitness (longevity reward)
    double age_fitness = fitness.age_seconds * config_.age_weight;

    // Energy integral (efficient foraging)
    double energy_fitness = fitness.energy_int_accum * config_.energy_weight;

    // Reproduction success
    double offspring_fitness = fitness.offspring_count * config_.offspring_weight;

    // Combined fitness
    fitness.last_fitness = age_fitness + energy_fitness + offspring_fitness;
}
```

#### 3. Selection (Truncation or Tournament)

**Truncation Selection**:
```cpp
// Sort by fitness (descending)
auto sorted = sort_by_fitness(population);

// Keep top N%
int survivors = population.size() * config_.survival_rate;
selected = sorted[0:survivors];

// Cull the rest
for (int i = survivors; i < population.size(); i++) {
    registry.destroy(sorted[i].entity);
}
```

**Tournament Selection**:
```cpp
std::vector<entt::entity> selected;
while (selected.size() < target_population) {
    // Pick K random individuals
    auto tournament = pick_random(population, config_.tournament_size);

    // Select best
    auto winner = max_by_fitness(tournament);
    selected.push_back(winner);
}
```

#### 4. Reproduction (Crossover)

```cpp
for (auto parent : selected_parents) {
    // Find mate within radius (same species preference)
    auto mate = find_best_mate(parent, species_radius, registry);

    if (mate.has_value()) {
        // Crossover genomes
        GenomeId offspring_id = GenomeOps::crossover(
            parent_genome,
            mate_genome,
            config_.repro_config,
            derive_seed(...)
        );

        // Mutate offspring
        GenomeOps::mutate(
            offspring_genome,
            config_.mutation_config,
            derive_seed(...),
            innovation_db_
        );

        // Insert into storage
        GenomeId final_id = storage.insert(offspring_genome);

        // Spawn phenotype
        entt::entity offspring_entity = registry.create();
        PhenotypeBuilder::build(final_id, registry, offspring_entity, storage);
    }
}
```

#### 5. Speciation

```cpp
// Run species clustering every generation
if (run_speciation_this_generation) {
    species_index_system->update_species(registry, storage);
}

// Species-based reproduction bias
for (auto& parent : selected_parents) {
    SpeciesId parent_species = classify_species(parent_genome_id);

    // Prefer same-species mates
    mate_search_radius = (parent_species == target_species)
        ? config_.same_species_radius
        : config_.cross_species_radius;
}
```

### SpeciesIndexSystem

**Purpose**: Classifies genomes into species using clustering algorithms, tracks species statistics.

**Algorithm**:

#### Clustering (K-means or Threshold-based)

**Threshold-based Clustering**:
```cpp
for (auto genome_a : all_genomes) {
    for (auto genome_b : all_genomes) {
        if (genome_a == genome_b) continue;

        // Compute compatibility distance
        double distance = GenomeOps::compatibility_distance(
            genome_a, genome_b, config_.distance_config
        );

        // If distance below threshold, same species
        if (distance < config_.speciation_threshold) {
            assign_same_species(genome_a, genome_b);
        }
    }
}
```

**K-means Clustering** (alternative):
```cpp
// Initialize K clusters
std::vector<Cluster> clusters = initialize_k_clusters(K);

// Iterate to convergence
while (!converged) {
    // Assign each genome to nearest cluster centroid
    for (auto genome : all_genomes) {
        Cluster nearest = find_nearest_cluster(genome, clusters);
        nearest.genomes.push_back(genome);
    }

    // Update centroids
    for (auto& cluster : clusters) {
        cluster.centroid = compute_centroid(cluster.genomes);
    }
}
```

**Species Statistics**:
- Population count per species
- Mean fitness per species
- Dominant traits per species

### TraitAnalysisSystem

**Purpose**: Computes trait statistics across population, tracks evolutionary trends.

**Metrics**:

**Trait Means**:
```cpp
for (auto trait_index : trait_range) {
    double sum = 0.0;
    int count = 0;

    for (auto genome : all_genomes) {
        double trait_value = extract_trait(genome, trait_index);
        sum += trait_value;
        count++;
    }

    trait_means[trait_index] = sum / count;
}
```

**Trait Distributions**:
- Histogram of trait values
- Standard deviation per trait
- Correlation matrix between traits

**Trait Diversity**:
- Shannon entropy of trait distributions
- Effective number of traits (1 / Σ(p_i²))

## Data Contracts

### EvolutionSystem ↔ GenomeStorage

**Read Contract**:
- EvolutionSystem reads all genomes for selection
- Reads parent genomes for crossover

**Write Contract**:
- EvolutionSystem inserts mutated offspring genomes
- No direct mutation (only via GenomeOps::mutate)

### EvolutionSystem ↔ SpeciesIndexSystem

**Read Contract**:
- EvolutionSystem queries species membership of genomes
- SpeciesIndexSystem reads GenomeOps::compatibility_distance

**Write Contract**:
- SpeciesIndexSystem updates species assignments
- EvolutionSystem uses species data for mate selection

### EvolutionSystem ↔ TelemetrySystem

**Data Flow**:
- Fitness values logged for post-hoc analysis
- Generation boundaries logged
- Species population trends logged

## Configuration

### EvolutionConfig

```cpp
struct EvolutionConfig {
    double generation_interval; // Seconds between generations (default: 60.0)
    double survival_rate; // Fraction to survive (default: 0.3)
    int tournament_size; // Tournament selection size (default: 5)
    double speciation_threshold; // Max distance for same species (default: 3.0)

    FitnessWeights fitness_weights; // Weights for age/energy/offspring

    ReproConfig repro_config; // Mutation/crossover rates
    MutationConfig mutation_config; // Structural/parameter mutation rates
};
```

## Evolutionary Strategies

### Selection Strategies

| Strategy | Description | Pros | Cons |
|----------|-------------|------|------|
| **Truncation** | Keep top N% by fitness | Fast | Loses diversity, premature convergence |
| **Tournament** | Random tournament, pick best | Preserves diversity | Slower, may select suboptimal |
| **Roulette Wheel** | Probability ∝ fitness | All have chance | Requires normalized fitness |
| **Lexicase** | Sort by multiple objectives | Multi-objective | Complex implementation |

### Crossover Strategies

| Strategy | Description |
|----------|-------------|
| **Uniform Crossover** | Random blend from two parents |
| **Arithmetic Crossover** | Weighted average of parents |
| **NEAT Crossover** | Innovation-based alignment, match/excess/disjoint handling |

### Mutation Strategies

**Parameter Mutation** (Gaussian):
```cpp
genome.weight += normal(0.0, mutation_sigma);
genome.bias += normal(0.0, mutation_sigma);
```

**Structural Mutation** (NEAT):
- Add node (new hidden neuron)
- Add connection (new synapse)
- Delete connection
- Toggle connection (enable/disable)

**Morphology Mutation**:
- Add limb (new body part)
- Modify limb (change size/density)
- Remove limb

## Performance Considerations

### Computational Cost

- **Fitness evaluation**: O(P) where P = population size
- **Selection**: O(P × log P) for sorting
- **Crossover**: O(N) where N = genome size (weights + topology)
- **Mutation**: O(N) for in-place modification
- **Speciation**: O(P²) for all-pairs distance (optimize with spatial indexing)

### Optimization Strategies

**Parallelizable Operations**:
- Fitness evaluation (independent per entity)
- Crossover (independent per parent pair)
- Mutation (independent per offspring)

**Cache Optimization**:
- Pre-compute trait vectors for clustering
- Reuse distance computations where possible

## Related Documentation

- [Genome Module](./genome.md) - Storage, mutation, crossover operations
- [Phenotype Module](./phenotype.md) - Building organisms from genomes
- [Components Module](./components.md) - FitnessComponent
- [Telemetry Module](./telemetry.md) - Fitness logging and analysis
- [Data Contracts](../data-contracts/genetics.md) - Inter-module data flow
