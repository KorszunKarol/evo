/**
 * @file species_index_system.h
 * @brief Species clustering and indexing system for genomes.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_ops.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Species identifier type.
 */
using SpeciesId = std::uint32_t;

/**
 * @brief System that clusters genomes into species using compatibility distance.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity Construction is O(1).
 * @note Maintains dynamic threshold to keep species count in target range.
 * @warning Requires GenomeStorage; runs periodically or on-demand.
 * @threadsafe @notthreadsafe.
 */
class SpeciesIndexSystem final : public ISystem {
public:
    /**
     * @brief Construct species index system.
     * @param storage Reference to genome storage (must outlive system).
     * @param config Reproduction configuration for distance computation.
     * @param target_species_count Target number of species to maintain.
     * @param initial_threshold Initial compatibility distance threshold.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Threshold auto-adjusts to maintain target species count.
     * @warning Storage must outlive system.
     * @threadsafe @notthreadsafe.
     */
    explicit SpeciesIndexSystem(genetics::GenomeStorage& storage,
                                 const genetics::ReproConfig& config,
                                 std::size_t target_species_count = 10,
                                 double initial_threshold = 3.0) noexcept;

    /**
     * @brief Update species assignments for all genomes.
     * @param context Simulation context (unused but required by interface).
     * @return None.
     * @throws None.
     * @complexity O(N²) where N = genome count (distance matrix computation).
     * @note Runs clustering algorithm and updates species assignments.
     * @warning Expensive for large populations; consider running periodically.
     * @threadsafe @notthreadsafe.
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Retrieve system identifier.
     * @param None.
     * @return std::string_view Literal name.
     * @throws None.
     * @complexity O(1).
     * @note Used for diagnostics.
     * @warning None.
     * @threadsafe Thread-safe for concurrent reads.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    /**
     * @brief Get species ID for a genome.
     * @param genome_id Genome identifier.
     * @return Species ID, or 0 if not found.
     * @throws None.
     * @complexity O(1) average.
     * @note Returns 0 for unknown genomes.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] SpeciesId get_species(genetics::GenomeId genome_id) const noexcept;

    /**
     * @brief Get current species count.
     * @param None.
     * @return Number of distinct species.
     * @throws None.
     * @complexity O(1).
     * @note Updated after each tick() call.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::size_t species_count() const noexcept { return species_count_; }

    /**
     * @brief Get current compatibility threshold.
     * @param None.
     * @return Current threshold value.
     * @throws None.
     * @complexity O(1).
     * @note Auto-adjusted to maintain target species count.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] double threshold() const noexcept { return threshold_; }

private:
    void UpdateClustering();
    void AdjustThreshold(std::size_t current_species_count);

    static constexpr std::string_view name_ = "species_index";
    genetics::GenomeStorage& storage_;
    genetics::ReproConfig config_;
    std::size_t target_species_count_;
    double threshold_;
    std::size_t species_count_{0};
    std::unordered_map<genetics::GenomeId, SpeciesId> species_map_{};
    std::vector<genetics::GenomeId> genome_list_{};
};

}  // namespace evolution::sim

