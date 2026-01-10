/**
 * @file mutation_ops.h
 * @brief NEAT structural mutation operators for topology evolution.
 */

#pragma once

#include <cstdint>
#include <string>

#include "evolution/genetics/innovation_database.h"
#include "evolution/genetics/rng.h"
#include "genome_generated.h"

namespace evolution::genetics {

/**
 * @brief Result of a mutation operation.
 */
struct MutationResult {
    bool success{false};
    std::string reason;
};

/**
 * @brief Configuration for structural mutation rates and constraints.
 */
struct StructuralMutationConfig {
    double add_node_prob{0.03};
    double add_conn_prob{0.05};
    double delete_conn_prob{0.02};
    double weight_init_sigma{1.0};
    double bias_init_sigma{0.5};
    bool allow_recurrent{true};
    std::uint32_t max_hidden_nodes{100};
    std::uint32_t max_connections{500};
};

/**
 * @brief Split an existing connection by inserting a new hidden node.
 * @param neat Target NEAT genome object (mutable).
 * @param innovations Global innovation database for consistent numbering.
 * @param rng Random number generator.
 * @return MutationResult indicating success or failure reason.
 * @throws None.
 * @complexity O(C) where C is connection count.
 * @note Selects a random enabled connection A→B, disables it, creates new node N,
 *       and adds connections A→N (weight 1.0) and N→B (original weight).
 * @warning Modifies genome in place.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] MutationResult mutate_add_node(
    evolution::genome::NEATT& neat,
    InnovationDatabase& innovations,
    Pcg32& rng) noexcept;

/**
 * @brief Add a new connection between two unconnected nodes.
 * @param neat Target NEAT genome object (mutable).
 * @param innovations Global innovation database for consistent numbering.
 * @param rng Random number generator.
 * @param config Mutation configuration including recurrence settings.
 * @return MutationResult indicating success or failure reason.
 * @throws None.
 * @complexity O(N² + C) where N is node count and C is connection count.
 * @note Attempts to find an unconnected (src, dst) pair. If allow_recurrent=false,
 *       ensures no cycle would be created (feed-forward constraint).
 * @warning Modifies genome in place.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] MutationResult mutate_add_connection(
    evolution::genome::NEATT& neat,
    InnovationDatabase& innovations,
    Pcg32& rng,
    const StructuralMutationConfig& config = {}) noexcept;

/**
 * @brief Remove a random enabled connection (pruning).
 * @param neat Target NEAT genome object (mutable).
 * @param rng Random number generator.
 * @return MutationResult indicating success or failure reason.
 * @throws None.
 * @complexity O(C) where C is connection count.
 * @note Removes connection entirely (not just disable). Helps prevent runaway
 *       complexity and allows re-evolution of that connection if beneficial.
 * @warning Modifies genome in place. May disconnect nodes.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] MutationResult mutate_delete_connection(
    evolution::genome::NEATT& neat,
    Pcg32& rng) noexcept;

/**
 * @brief Toggle the enabled state of a random connection.
 * @param neat Target NEAT genome object (mutable).
 * @param rng Random number generator.
 * @return MutationResult indicating success or failure reason.
 * @throws None.
 * @complexity O(C) where C is connection count.
 * @note Flips enabled flag. Used for exploration without permanent deletion.
 * @warning Modifies genome in place.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] MutationResult mutate_toggle_connection(
    evolution::genome::NEATT& neat,
    Pcg32& rng) noexcept;

/**
 * @brief Apply all structural mutations with configured probabilities.
 * @param neat Target NEAT genome object (mutable).
 * @param innovations Global innovation database.
 * @param rng Random number generator.
 * @param config Mutation configuration.
 * @return Number of mutations applied.
 * @throws None.
 * @complexity O(N² + C).
 * @note Applies AddNode, AddConnection, DeleteConnection based on probabilities.
 * @warning Modifies genome in place.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] std::uint32_t apply_structural_mutations(
    evolution::genome::NEATT& neat,
    InnovationDatabase& innovations,
    Pcg32& rng,
    const StructuralMutationConfig& config) noexcept;

}  // namespace evolution::genetics

