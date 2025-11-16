/**
 * @file genome_types.h
 * @brief Shared genetics type aliases and POD structs.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace evolution::genetics {

/**
 * @brief Canonical identifier for genomes stored in GenomeStorage.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1) alias.
 * @note Values are computed via stable hashing of serialized genomes.
 * @warning Uniqueness is probabilistic; ensure collision handling at higher layers.
 * @threadsafe @notthreadsafe Dependent on usage; treat as value type.
 */
using GenomeId = std::uint64_t;

/**
 * @brief Derived physical and metabolic traits computed from a genome.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1) access for all fields.
 * @note Values are deterministic functions of raw genome fields.
 * @warning Trait values assume valid genome inputs; sanitize genomes before use.
 * @threadsafe @notthreadsafe Treat as immutable value once constructed.
 */
struct DerivedTraits {
    /// @brief Total body mass in kilograms.
    double mass{1.0};
    /// @brief Basal metabolic consumption per second.
    double basal_rate{1.0};
    /// @brief Brain-specific energetic cost applied during inference.
    double brain_cost{0.0};
    /// @brief Total parameter count proxy used for telemetry and brain energy scaling.
    double brain_params{0.0};
    /// @brief Number of behavioural modules encoded in the genome.
    std::size_t module_count{1};
    /// @brief Latent trait embedding derived deterministically from genome morphology/colouring.
    std::array<double, 8> trait_latent{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
};

/**
 * @brief Result object returned by PhenotypeBuilder::build.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1) storage.
 * @note The traits field is meaningful only when ok == true.
 * @warning Inspect msg when ok == false to diagnose build failures.
 * @threadsafe @notthreadsafe Mutate on a single thread; treat as plain data.
 */
struct PhenotypeBuildResult {
    /// @brief Indicates whether phenotype construction succeeded.
    bool ok{false};
    /// @brief Diagnostics or failure reason when ok == false.
    std::string msg;
    /// @brief Derived traits snapshot generated during build.
    DerivedTraits traits{};
};

}  // namespace evolution::genetics


