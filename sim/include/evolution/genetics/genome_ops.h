/**
 * @file genome_ops.h
 * @brief Mutation, crossover, and distance metrics for genomes.
 */

#pragma once

#include <cstdint>

#include "evolution/genetics/genome_types.h"
#include "evolution/genetics/rng.h"
#include "genome_generated.h"

namespace evolution::genetics {

class InnovationDatabase;

/**
 * @brief Configuration parameters controlling stochastic genetic operations.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1).
 * @note Values tuned for prototype populations; expect future iteration.
 * @warning Rates outside [0,1] lead to undefined behaviour in mutation routines.
 * @threadsafe @notthreadsafe Mutate values only on simulation thread.
 */
struct ReproConfig {
    double mutate_rate_struct{0.05};
    double mutate_rate_param{0.2};
    double weight_sigma{0.2};
    double neat_excess_weight{1.0};
    double neat_disjoint_weight{1.0};
    double neat_weight_diff_weight{0.4};
    double neat_body_weight{0.3};
};

/**
 * @brief Apply deterministic mutation to a genome.
 * @param genome Source genome object (consumed by value).
 * @param config Mutation configuration parameters.
 * @param seed Deterministic seed for RNG.
 * @return Mutated genome object.
 * @throws None.
 * @complexity O(N) where N equals genome weight count.
 * @note The returned genome has id cleared to 0; callers must re-hash via storage insertion.
 * @warning Placeholder implementation performs minimal jitter; future milestones extend behaviour.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] evolution::genome::GenomeT mutate(evolution::genome::GenomeT genome,
                                                const ReproConfig& config,
                                                std::uint64_t seed) noexcept;

/**
 * @brief Apply mutation with structural topology changes.
 * @param genome Source genome object (consumed by value).
 * @param config Mutation configuration parameters.
 * @param seed Deterministic seed for RNG.
 * @param innovations Innovation database for structural mutations (required for NEAT topology changes).
 * @return Mutated genome object.
 * @throws None.
 * @complexity O(N² + C) where N = node count, C = connection count.
 * @note Enables AddNode/AddConnection mutations when innovations is non-null.
 * @warning Modifies topology; crossover compatibility depends on innovation tracking.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] evolution::genome::GenomeT mutate(evolution::genome::GenomeT genome,
                                                const ReproConfig& config,
                                                std::uint64_t seed,
                                                InnovationDatabase& innovations) noexcept;

/**
 * @brief Combine two parent genomes using deterministic crossover.
 * @param a First parent genome.
 * @param b Second parent genome.
 * @param config Reproduction configuration.
 * @param seed Deterministic RNG seed.
 * @return Offspring genome with id cleared.
 * @throws None.
 * @complexity O(N) with N = max(a.metric, b.metric).
 * @note Placeholder implementation currently selects dominant parent features.
 * @warning Revisit after NEAT innovation table is implemented.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] evolution::genome::GenomeT crossover(const evolution::genome::GenomeT& a,
                                                   const evolution::genome::GenomeT& b,
                                                   const ReproConfig& config,
                                                   std::uint64_t seed) noexcept;

/**
 * @brief Compute compatibility distance between two genomes for speciation.
 * @param a First genome (immutable table view).
 * @param b Second genome.
 * @param config Configuration weights.
 * @return Symmetric non-negative distance metric.
 * @throws None.
 * @complexity O(N + M) where N/M correspond to edge counts of each genome.
 * @note Placeholder returns zero for identical brain kinds and unity otherwise.
 * @warning Will be replaced with full NEAT distance metric in Task 4.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] double compatibility_distance(const evolution::genome::Genome& a,
                                            const evolution::genome::Genome& b,
                                            const ReproConfig& config) noexcept;

}  // namespace evolution::genetics


