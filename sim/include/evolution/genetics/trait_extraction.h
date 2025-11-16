/**
 * @file trait_extraction.h
 * @brief Utilities for extracting trait vectors from genomes for preference/mating.
 */

#pragma once

#include <array>
#include <cstdint>

#include "genome_generated.h"

namespace evolution::genetics {

/**
 * @brief Extract a normalized trait vector from a genome for preference evaluation.
 * @param genome Immutable genome table view.
 * @return Array of 8 normalized trait values.
 * @throws None.
 * @complexity O(1) - extracts cached or computed values.
 * @note Traits: size_x, size_y, size_z, mass_density, color_r, color_g, color_b, brain_size
 * @warning Values normalized to [0,1] range; may be clamped.
 * @threadsafe @notthreadsafe Read-only but not synchronized.
 */
[[nodiscard]] std::array<double, 8> ExtractTraitVector(const evolution::genome::Genome& genome) noexcept;

/**
 * @brief Extract trait vector from native genome object.
 * @param genome Native genome object.
 * @return Array of 8 normalized trait values.
 * @throws None.
 * @complexity O(1).
 * @note Same normalization as table-based version.
 * @warning None.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] std::array<double, 8> ExtractTraitVector(const evolution::genome::GenomeT& genome) noexcept;

/**
 * @brief Compute cosine distance between two trait vectors.
 * @param a First trait vector.
 * @param b Second trait vector.
 * @return Distance in [0, 2] range (0 = identical, 2 = opposite).
 * @throws None.
 * @complexity O(N) where N = vector size (8).
 * @note Normalized vectors yield distance in [0, 2].
 * @warning Input vectors should be normalized; no validation performed.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] double TraitCosineDistance(const std::array<double, 8>& a,
                                         const std::array<double, 8>& b) noexcept;

}  // namespace evolution::genetics

