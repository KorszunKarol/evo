/**
 * @file derived_traits.h
 * @brief Helper functions computing phenotype traits from genomes.
 */

#pragma once

#include "evolution/genetics/genome_types.h"
#include "genome_generated.h"

namespace evolution::genetics {

/**
 * @brief Derive physical and metabolic traits from an immutable genome table.
 * @param genome FlatBuffers table view of the genome.
 * @return Deterministic trait bundle used by phenotype builder.
 * @throws None.
 * @complexity O(N) where N equals number of brain weights/edges.
 * @note Does not mutate the provided genome; safe for shared usage.
 * @warning Expects validated genome contents; undefined behaviour otherwise.
 * @threadsafe @notthreadsafe Input pointer is read-only yet not synchronized.
 */
[[nodiscard]] DerivedTraits ComputeDerivedTraits(const evolution::genome::Genome& genome) noexcept;

/**
 * @brief Derive traits from a mutable native genome object.
 * @param genome Native C++ object produced by FlatBuffers object API.
 * @return Deterministic trait bundle.
 * @throws None.
 * @complexity O(N) with N mirroring the serialized form.
 * @note Convenience overload for callers constructing genomes via object API.
 * @warning The object must respect schema invariants; no validation performed.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] DerivedTraits ComputeDerivedTraits(const evolution::genome::GenomeT& genome) noexcept;

}  // namespace evolution::genetics


