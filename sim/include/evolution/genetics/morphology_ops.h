/**
 * @file morphology_ops.h
 * @brief Morphology mutation operators for body evolution.
 */

#pragma once

#include <cstdint>
#include <string>

#include "evolution/genetics/rng.h"
#include "genome_generated.h"

namespace evolution::genetics {

/**
 * @brief Result of a morphology mutation operation.
 */
struct MorphologyResult {
    bool success{false};
    std::string reason;
};

/**
 * @brief Constraints for morphology mutations.
 */
struct MorphologyConstraints {
    std::uint32_t max_limbs{6};
    std::uint32_t max_depth{4};
    double min_segment_size{0.05};
    double max_segment_size{1.5};
    double min_density{100.0};
    double max_density{2000.0};
    double size_mutation_sigma{0.1};
    double density_mutation_sigma{0.1};
    double joint_limit_mutation_sigma{0.2};
};

/**
 * @brief Count total limbs (nodes) in body tree.
 * @param body Root body node.
 * @return Total node count including root.
 * @throws None.
 * @complexity O(N) where N = node count.
 * @note Recursive count of body tree.
 * @warning None.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] std::uint32_t count_body_nodes(
    const evolution::genome::BodyNodeT& body) noexcept;

/**
 * @brief Get maximum depth of body tree.
 * @param body Root body node.
 * @return Maximum depth (root = 1).
 * @throws None.
 * @complexity O(N) where N = node count.
 * @note None.
 * @warning None.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] std::uint32_t get_body_depth(
    const evolution::genome::BodyNodeT& body) noexcept;

/**
 * @brief Add a new limb segment to a random existing node.
 * @param body Root body node (mutable).
 * @param rng Random number generator.
 * @param constraints Morphology constraints.
 * @return MorphologyResult indicating success or failure.
 * @throws None.
 * @complexity O(N) where N = node count.
 * @note Creates new child node with random size and joint.
 * @warning Modifies body in place.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] MorphologyResult mutate_add_limb(
    evolution::genome::BodyNodeT& body,
    Pcg32& rng,
    const MorphologyConstraints& constraints = {}) noexcept;

/**
 * @brief Modify properties of a random limb segment.
 * @param body Root body node (mutable).
 * @param rng Random number generator.
 * @param constraints Morphology constraints.
 * @return MorphologyResult indicating success or failure.
 * @throws None.
 * @complexity O(N) where N = node count.
 * @note Modifies size, density, or joint limits.
 * @warning Modifies body in place.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] MorphologyResult mutate_modify_limb(
    evolution::genome::BodyNodeT& body,
    Pcg32& rng,
    const MorphologyConstraints& constraints = {}) noexcept;

/**
 * @brief Remove a random non-root limb segment.
 * @param body Root body node (mutable).
 * @param rng Random number generator.
 * @return MorphologyResult indicating success or failure.
 * @throws None.
 * @complexity O(N) where N = node count.
 * @note Removes node and all its children.
 * @warning Modifies body in place. May disconnect subtrees.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] MorphologyResult mutate_remove_limb(
    evolution::genome::BodyNodeT& body,
    Pcg32& rng) noexcept;

/**
 * @brief Apply morphology mutations with configured probabilities.
 * @param body Root body node (mutable).
 * @param rng Random number generator.
 * @param add_prob Probability of adding a limb.
 * @param modify_prob Probability of modifying a limb.
 * @param remove_prob Probability of removing a limb.
 * @param constraints Morphology constraints.
 * @return Number of mutations applied.
 * @throws None.
 * @complexity O(N).
 * @note None.
 * @warning Modifies body in place.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] std::uint32_t apply_morphology_mutations(
    evolution::genome::BodyNodeT& body,
    Pcg32& rng,
    double add_prob = 0.02,
    double modify_prob = 0.1,
    double remove_prob = 0.01,
    const MorphologyConstraints& constraints = {}) noexcept;

}  // namespace evolution::genetics

