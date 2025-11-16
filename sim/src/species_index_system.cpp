#include "evolution/sim/species_index_system.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include <spdlog/spdlog.h>

namespace evolution::sim {

SpeciesIndexSystem::SpeciesIndexSystem(genetics::GenomeStorage& storage,
                                       const genetics::ReproConfig& config,
                                       std::size_t target_species_count,
                                       double initial_threshold) noexcept
    : storage_(storage)
    , config_(config)
    , target_species_count_(target_species_count)
    , threshold_(initial_threshold) {}

void SpeciesIndexSystem::tick(SimulationContext& context) {
    (void)context;
    UpdateClustering();
}

void SpeciesIndexSystem::UpdateClustering() {
    // Get all genome IDs
    genome_list_ = storage_.ids();
    if (genome_list_.empty()) {
        species_map_.clear();
        species_count_ = 0;
        return;
    }

    // Simple threshold-based clustering
    species_map_.clear();
    SpeciesId next_species_id = 1;

    for (genetics::GenomeId genome_id : genome_list_) {
        const auto* genome = storage_.get(genome_id);
        if (genome == nullptr) {
            continue;
        }

        // Find closest existing species
        SpeciesId assigned_species = 0;
        double min_distance = threshold_;

        for (const auto& [existing_id, existing_species] : species_map_) {
            const auto* existing_genome = storage_.get(existing_id);
            if (existing_genome == nullptr) {
                continue;
            }

            const double distance = genetics::compatibility_distance(*genome, *existing_genome, config_);
            if (distance < min_distance) {
                min_distance = distance;
                assigned_species = existing_species;
            }
        }

        // Assign to existing species or create new one
        if (assigned_species == 0) {
            assigned_species = next_species_id++;
        }
        species_map_[genome_id] = assigned_species;
    }

    species_count_ = next_species_id - 1;

    // Adjust threshold to maintain target species count
    AdjustThreshold(species_count_);

    // Log statistics
    if (species_count_ > 0) {
        std::unordered_map<SpeciesId, std::size_t> species_sizes;
        for (const auto& [_, species_id] : species_map_) {
            species_sizes[species_id]++;
        }

        std::size_t max_size = 0;
        std::size_t min_size = genome_list_.size();
        for (const auto& [_, size] : species_sizes) {
            max_size = std::max(max_size, size);
            min_size = std::min(min_size, size);
        }

        spdlog::info("SpeciesIndexSystem: {} species, threshold={:.3f}, sizes=[{}, {}]",
                     species_count_, threshold_, min_size, max_size);
    }
}

void SpeciesIndexSystem::AdjustThreshold(std::size_t current_species_count) {
    constexpr double kAdjustRate = 0.1;
    constexpr double kMinThreshold = 0.5;
    constexpr double kMaxThreshold = 10.0;

    if (current_species_count < target_species_count_) {
        // Too few species: decrease threshold (tighter clustering)
        threshold_ = std::max(kMinThreshold, threshold_ * (1.0 - kAdjustRate));
    } else if (current_species_count > target_species_count_ * 1.5) {
        // Too many species: increase threshold (looser clustering)
        threshold_ = std::min(kMaxThreshold, threshold_ * (1.0 + kAdjustRate));
    }
}

SpeciesId SpeciesIndexSystem::get_species(genetics::GenomeId genome_id) const noexcept {
    const auto it = species_map_.find(genome_id);
    return it != species_map_.end() ? it->second : 0;
}

}  // namespace evolution::sim

