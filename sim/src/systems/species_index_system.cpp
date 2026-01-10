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
    genome_list_ = storage_.ids();
    if (genome_list_.empty()) {
        species_map_.clear();
        species_list_.clear();
        return;
    }

    std::unordered_map<SpeciesId, genetics::GenomeId> old_representatives;
    for (const auto& species : species_list_) {
        old_representatives[species.id] = species.representative_id;
    }

    species_map_.clear();
    SpeciesId next_species_id = species_list_.empty() ? 1 : 
        species_list_.back().id + 1;

    for (genetics::GenomeId genome_id : genome_list_) {
        const auto* genome = storage_.get(genome_id);
        if (genome == nullptr) {
            continue;
        }

        SpeciesId assigned_species = 0;
        double min_distance = threshold_;

        for (const auto& [species_id, rep_id] : old_representatives) {
            const auto* rep_genome = storage_.get(rep_id);
            if (rep_genome == nullptr) {
                continue;
            }

            const double distance = genetics::compatibility_distance(*genome, *rep_genome, config_);
            if (distance < min_distance) {
                min_distance = distance;
                assigned_species = species_id;
            }
        }

        if (assigned_species == 0) {
            assigned_species = next_species_id++;
            old_representatives[assigned_species] = genome_id;
        }
        species_map_[genome_id] = assigned_species;
    }

    RebuildSpeciesList();
    AdjustThreshold(species_list_.size());

    if (!species_list_.empty()) {
        std::size_t max_size = 0;
        std::size_t min_size = genome_list_.size();
        for (const auto& species : species_list_) {
            max_size = std::max(max_size, species.members.size());
            min_size = std::min(min_size, species.members.size());
        }

        spdlog::info("SpeciesIndexSystem: {} species, threshold={:.3f}, sizes=[{}, {}]",
                     species_list_.size(), threshold_, min_size, max_size);
    }
}

void SpeciesIndexSystem::RebuildSpeciesList() {
    std::unordered_map<SpeciesId, std::vector<genetics::GenomeId>> species_members;
    for (const auto& [genome_id, species_id] : species_map_) {
        species_members[species_id].push_back(genome_id);
    }

    std::unordered_map<SpeciesId, Species> old_species_data;
    for (const auto& species : species_list_) {
        old_species_data[species.id] = species;
    }

    species_list_.clear();
    species_list_.reserve(species_members.size());

    for (auto& [species_id, members] : species_members) {
        Species new_species{};
        new_species.id = species_id;
        new_species.members = std::move(members);
        
        if (!new_species.members.empty()) {
            new_species.representative_id = new_species.members.front();
        }

        auto old_it = old_species_data.find(species_id);
        if (old_it != old_species_data.end()) {
            new_species.best_fitness = old_it->second.best_fitness;
            new_species.stagnation_generations = old_it->second.stagnation_generations;
            new_species.age_generations = old_it->second.age_generations;
        }

        species_list_.push_back(std::move(new_species));
    }

    std::sort(species_list_.begin(), species_list_.end(),
              [](const Species& a, const Species& b) {
                  return a.id < b.id;
              });
}

void SpeciesIndexSystem::AdjustThreshold(std::size_t current_species_count) {
    constexpr double kAdjustRate = 0.1;
    constexpr double kMinThreshold = 0.5;
    constexpr double kMaxThreshold = 10.0;

    if (current_species_count < target_species_count_) {
        threshold_ = std::max(kMinThreshold, threshold_ * (1.0 - kAdjustRate));
    } else if (current_species_count > target_species_count_ * 1.5) {
        threshold_ = std::min(kMaxThreshold, threshold_ * (1.0 + kAdjustRate));
    }
}

SpeciesId SpeciesIndexSystem::get_species(genetics::GenomeId genome_id) const noexcept {
    const auto it = species_map_.find(genome_id);
    return it != species_map_.end() ? it->second : 0;
}

double SpeciesIndexSystem::get_adjusted_fitness(genetics::GenomeId genome_id,
                                                 double raw_fitness) const noexcept {
    const SpeciesId species_id = get_species(genome_id);
    if (species_id == 0) {
        return raw_fitness;
    }

    for (const auto& species : species_list_) {
        if (species.id == species_id) {
            return species.adjusted_fitness(raw_fitness);
        }
    }

    return raw_fitness;
}

void SpeciesIndexSystem::update_stagnation(
    const std::unordered_map<genetics::GenomeId, double>& fitness_map) {
    for (auto& species : species_list_) {
        double species_best = 0.0;
        double total_adjusted = 0.0;

        for (genetics::GenomeId member_id : species.members) {
            auto it = fitness_map.find(member_id);
            if (it != fitness_map.end()) {
                species_best = std::max(species_best, it->second);
                total_adjusted += species.adjusted_fitness(it->second);
            }
        }

        species.total_adjusted_fitness = total_adjusted;

        if (species_best > species.best_fitness) {
            species.best_fitness = species_best;
            species.stagnation_generations = 0;
        } else {
            ++species.stagnation_generations;
        }
    }
}

std::size_t SpeciesIndexSystem::cull_stagnant_species(std::uint32_t max_stagnation,
                                                       std::size_t keep_minimum) {
    if (species_list_.size() <= keep_minimum) {
        return 0;
    }

    std::vector<Species> sorted_species = species_list_;
    std::sort(sorted_species.begin(), sorted_species.end(),
              [](const Species& a, const Species& b) {
                  return a.best_fitness > b.best_fitness;
              });

    std::unordered_set<SpeciesId> protected_ids;
    for (std::size_t i = 0; i < keep_minimum && i < sorted_species.size(); ++i) {
        protected_ids.insert(sorted_species[i].id);
    }

    std::size_t removed_count = 0;
    auto it = species_list_.begin();
    while (it != species_list_.end()) {
        if (it->is_stagnant(max_stagnation) && 
            protected_ids.find(it->id) == protected_ids.end() &&
            species_list_.size() - removed_count > keep_minimum) {
            
            for (genetics::GenomeId member_id : it->members) {
                species_map_.erase(member_id);
            }
            
            it = species_list_.erase(it);
            ++removed_count;
        } else {
            ++it;
        }
    }

    if (removed_count > 0) {
        spdlog::info("SpeciesIndexSystem: culled {} stagnant species", removed_count);
    }

    return removed_count;
}

void SpeciesIndexSystem::advance_generation() {
    ++generation_;
    for (auto& species : species_list_) {
        ++species.age_generations;
    }
}

}  // namespace evolution::sim

