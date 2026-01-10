#include "evolution/sim/evolution_system.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include <spdlog/spdlog.h>

#include "evolution/genetics/rng.h"

namespace evolution::sim {

EvolutionSystem::EvolutionSystem(
    genetics::GenomeStorage& storage,
    genetics::InnovationDatabase& innovations,
    SpeciesIndexSystem& species,
    const EvolutionConfig& config,
    const genetics::ReproConfig& repro_config,
    std::uint64_t global_seed) noexcept
    : storage_(storage)
    , innovations_(innovations)
    , species_system_(species)
    , config_(config)
    , repro_config_(repro_config)
    , global_seed_(global_seed) {}

void EvolutionSystem::tick(SimulationContext& context) {
    (void)context;
}

void EvolutionSystem::set_fitness(
    const std::unordered_map<genetics::GenomeId, double>& fitness_map) {
    fitness_map_ = fitness_map;
}

void EvolutionSystem::initialize_population(
    const evolution::genome::GenomeT& template_genome,
    std::size_t count) {
    population_.clear();
    population_.reserve(count);

    genetics::Pcg32 rng(global_seed_ ^ 0x494E4954504F5055ULL);

    for (std::size_t i = 0; i < count; ++i) {
        auto genome = evolution::genome::GenomeT(template_genome);
        genome.id = 0;
        genome.generation = 0;
        genome.rng_seed = rng.next_u64();

        genome = genetics::mutate(std::move(genome), repro_config_, rng.next_u64(), innovations_);

        const genetics::GenomeId id = storage_.insert(std::move(genome));
        population_.push_back(id);
    }

    spdlog::info("EvolutionSystem: initialized population with {} genomes", count);
}

void EvolutionSystem::ComputeAdjustedFitness() {
    ranked_population_.clear();
    ranked_population_.reserve(population_.size());

    for (genetics::GenomeId id : population_) {
        RankedGenome ranked{};
        ranked.id = id;
        
        auto it = fitness_map_.find(id);
        ranked.raw_fitness = it != fitness_map_.end() ? it->second : 0.0;
        
        ranked.species_id = species_system_.get_species(id);
        ranked.adjusted_fitness = species_system_.get_adjusted_fitness(id, ranked.raw_fitness);
        
        ranked_population_.push_back(ranked);
    }

    std::sort(ranked_population_.begin(), ranked_population_.end(),
              [](const RankedGenome& a, const RankedGenome& b) {
                  return a.raw_fitness > b.raw_fitness;
              });

    if (!ranked_population_.empty()) {
        champion_id_ = ranked_population_.front().id;
    }
}

std::vector<genetics::GenomeId> EvolutionSystem::SelectParents(std::size_t count) {
    std::vector<genetics::GenomeId> parents;
    parents.reserve(count);

    const auto& species_list = species_system_.species_list();
    if (species_list.empty()) {
        return parents;
    }

    double total_adjusted = 0.0;
    for (const auto& species : species_list) {
        total_adjusted += species.total_adjusted_fitness;
    }

    if (total_adjusted <= 0.0) {
        total_adjusted = 1.0;
    }

    genetics::Pcg32 rng(global_seed_ ^ (static_cast<std::uint64_t>(generation_) << 32) ^ 0x53454C454354ULL);

    for (std::size_t i = 0; i < count; ++i) {
        double target = rng.next_unit() * total_adjusted;
        double cumulative = 0.0;
        
        for (const auto& species : species_list) {
            cumulative += species.total_adjusted_fitness;
            if (cumulative >= target && !species.members.empty()) {
                genetics::GenomeId selected = SelectFromSpecies(species);
                parents.push_back(selected);
                break;
            }
        }
        
        if (parents.size() <= i) {
            const auto& fallback_species = species_list[rng.next_u32() % species_list.size()];
            if (!fallback_species.members.empty()) {
                parents.push_back(fallback_species.members[rng.next_u32() % fallback_species.members.size()]);
            }
        }
    }

    return parents;
}

genetics::GenomeId EvolutionSystem::SelectFromSpecies(const Species& species) {
    if (species.members.empty()) {
        return 0;
    }

    std::vector<std::pair<genetics::GenomeId, double>> members_with_fitness;
    members_with_fitness.reserve(species.members.size());

    for (genetics::GenomeId id : species.members) {
        auto it = fitness_map_.find(id);
        double fitness = it != fitness_map_.end() ? it->second : 0.0;
        members_with_fitness.emplace_back(id, fitness);
    }

    std::sort(members_with_fitness.begin(), members_with_fitness.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    genetics::Pcg32 rng(global_seed_ ^ 0x535045434945ULL ^ species.id);
    
    const std::size_t top_count = std::max(std::size_t{1}, members_with_fitness.size() / 2);
    return members_with_fitness[rng.next_u32() % top_count].first;
}

genetics::GenomeId EvolutionSystem::ProduceOffspring(genetics::GenomeId parent_a,
                                                      genetics::GenomeId parent_b) {
    const auto* genome_a = storage_.get(parent_a);
    const auto* genome_b = storage_.get(parent_b);

    if (!genome_a || !genome_b) {
        return 0;
    }

    genetics::Pcg32 rng(global_seed_ ^ parent_a ^ parent_b ^ 
                        (static_cast<std::uint64_t>(generation_) << 48));

    auto genome_a_obj = genome_a->UnPack();
    auto genome_b_obj = genome_b->UnPack();

    evolution::genome::GenomeT offspring;

    if (rng.next_unit() < config_.crossover_rate && parent_a != parent_b) {
        offspring = genetics::crossover(*genome_a_obj, *genome_b_obj, repro_config_, rng.next_u64());
    } else {
        auto it_a = fitness_map_.find(parent_a);
        auto it_b = fitness_map_.find(parent_b);
        double fit_a = it_a != fitness_map_.end() ? it_a->second : 0.0;
        double fit_b = it_b != fitness_map_.end() ? it_b->second : 0.0;
        
        if (fit_a >= fit_b) {
            offspring = evolution::genome::GenomeT(*genome_a_obj);
        } else {
            offspring = evolution::genome::GenomeT(*genome_b_obj);
        }
    }

    if (rng.next_unit() < config_.mutation_rate) {
        offspring = genetics::mutate(std::move(offspring), repro_config_, rng.next_u64(), innovations_);
    }

    offspring.generation = generation_ + 1;
    offspring.parents = {parent_a, parent_b};

    return storage_.insert(std::move(offspring));
}

void EvolutionSystem::ReplacePopulation(const std::vector<genetics::GenomeId>& offspring) {
    population_ = offspring;
}

GenerationStats EvolutionSystem::advance_generation() {
    ComputeAdjustedFitness();

    species_system_.update_stagnation(fitness_map_);
    species_system_.cull_stagnant_species(config_.max_stagnation, config_.keep_minimum_species);

    GenerationStats stats{};
    stats.generation = generation_;
    stats.population_size = population_.size();
    stats.species_count = species_system_.active_species_count();

    if (!ranked_population_.empty()) {
        stats.best_fitness = ranked_population_.front().raw_fitness;
        stats.best_adjusted_fitness = ranked_population_.front().adjusted_fitness;
        stats.champion_id = ranked_population_.front().id;

        double sum = 0.0;
        for (const auto& r : ranked_population_) {
            sum += r.raw_fitness;
        }
        stats.mean_fitness = sum / static_cast<double>(ranked_population_.size());
    }

    std::vector<genetics::GenomeId> new_population;
    new_population.reserve(config_.population_size);

    const std::size_t elite_count = std::min(
        static_cast<std::size_t>(config_.elite_count),
        ranked_population_.size());
    
    for (std::size_t i = 0; i < elite_count; ++i) {
        new_population.push_back(ranked_population_[i].id);
    }

    const std::size_t offspring_needed = config_.population_size - new_population.size();
    auto parents = SelectParents(offspring_needed * 2);

    genetics::Pcg32 rng(global_seed_ ^ (static_cast<std::uint64_t>(generation_) << 32) ^ 0x4F46465350ULL);

    for (std::size_t i = 0; i < offspring_needed && i * 2 + 1 < parents.size(); ++i) {
        genetics::GenomeId parent_a = parents[i * 2];
        genetics::GenomeId parent_b = parents[i * 2 + 1];

        if (config_.interspecies_mating && rng.next_unit() < config_.interspecies_mating_rate) {
            parent_b = parents[rng.next_u32() % parents.size()];
        }

        genetics::GenomeId offspring_id = ProduceOffspring(parent_a, parent_b);
        if (offspring_id != 0) {
            new_population.push_back(offspring_id);
        }
    }

    while (new_population.size() < config_.population_size && !parents.empty()) {
        genetics::GenomeId parent = parents[rng.next_u32() % parents.size()];
        genetics::GenomeId offspring_id = ProduceOffspring(parent, parent);
        if (offspring_id != 0) {
            new_population.push_back(offspring_id);
        }
    }

    ReplacePopulation(new_population);

    ++generation_;
    species_system_.advance_generation();

    spdlog::info("EvolutionSystem: gen {} - pop={}, species={}, best={:.4f}, mean={:.4f}",
                 stats.generation, stats.population_size, stats.species_count,
                 stats.best_fitness, stats.mean_fitness);

    return stats;
}

}  // namespace evolution::sim

