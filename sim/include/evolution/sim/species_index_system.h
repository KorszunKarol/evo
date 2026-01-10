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
 * @brief Species data structure holding members and fitness tracking.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1) for most operations.
 * @note Tracks representative genome, members, fitness history, and stagnation.
 * @warning None.
 * @threadsafe @notthreadsafe.
 */
struct Species {
    SpeciesId id{0};
    genetics::GenomeId representative_id{0};
    std::vector<genetics::GenomeId> members{};
    double best_fitness{0.0};
    double total_adjusted_fitness{0.0};
    std::uint32_t stagnation_generations{0};
    std::uint32_t age_generations{0};

    /**
     * @brief Compute adjusted fitness for explicit fitness sharing.
     * @param raw_fitness Raw fitness value.
     * @return Adjusted fitness = raw_fitness / species_size.
     * @throws None.
     * @complexity O(1).
     * @note Returns raw_fitness if species is empty (edge case protection).
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] double adjusted_fitness(double raw_fitness) const noexcept {
        return members.empty() ? raw_fitness : raw_fitness / static_cast<double>(members.size());
    }

    /**
     * @brief Check if species is stagnant.
     * @param max_stagnation Maximum generations without improvement.
     * @return True if stagnation exceeds threshold.
     * @throws None.
     * @complexity O(1).
     * @note None.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] bool is_stagnant(std::uint32_t max_stagnation = 15) const noexcept {
        return stagnation_generations >= max_stagnation;
    }
};

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
     * @brief Get adjusted fitness for a genome.
     * @param genome_id Genome identifier.
     * @param raw_fitness Raw fitness value.
     * @return Adjusted fitness (raw / species_size) or raw if not in species.
     * @throws None.
     * @complexity O(1) average.
     * @note Implements explicit fitness sharing.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] double get_adjusted_fitness(genetics::GenomeId genome_id,
                                               double raw_fitness) const noexcept;

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
    [[nodiscard]] std::size_t active_species_count() const noexcept { return species_list_.size(); }

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

    /**
     * @brief Get all species.
     * @param None.
     * @return Const reference to species list.
     * @throws None.
     * @complexity O(1).
     * @note None.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] const std::vector<Species>& species_list() const noexcept { return species_list_; }

    /**
     * @brief Update stagnation tracking for all species.
     * @param fitness_map Map of genome_id -> raw_fitness.
     * @return None.
     * @throws None.
     * @complexity O(S * M) where S = species count, M = average members.
     * @note Should be called after fitness evaluation.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    void update_stagnation(const std::unordered_map<genetics::GenomeId, double>& fitness_map);

    /**
     * @brief Remove stagnant species below extinction threshold.
     * @param max_stagnation Maximum generations without improvement before extinction.
     * @param keep_minimum Minimum number of species to preserve.
     * @return Number of species removed.
     * @throws None.
     * @complexity O(S) where S = species count.
     * @note Protects top species from extinction.
     * @warning May dramatically reduce species count.
     * @threadsafe @notthreadsafe.
     */
    std::size_t cull_stagnant_species(std::uint32_t max_stagnation = 15,
                                       std::size_t keep_minimum = 2);

    /**
     * @brief Advance generation counter and update species ages.
     * @param None.
     * @return None.
     * @throws None.
     * @complexity O(S).
     * @note Should be called at end of each generation.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    void advance_generation();

private:
    void UpdateClustering();
    void AdjustThreshold(std::size_t current_species_count);
    void RebuildSpeciesList();

    static constexpr std::string_view name_ = "species_index";
    genetics::GenomeStorage& storage_;
    genetics::ReproConfig config_;
    std::size_t target_species_count_;
    double threshold_;
    std::uint32_t generation_{0};
    std::unordered_map<genetics::GenomeId, SpeciesId> species_map_{};
    std::vector<genetics::GenomeId> genome_list_{};
    std::vector<Species> species_list_{};
};

}  // namespace evolution::sim

