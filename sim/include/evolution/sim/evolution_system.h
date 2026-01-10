/**
 * @file evolution_system.h
 * @brief Generational evolution system for NEAT-based population management.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/innovation_database.h"
#include "evolution/sim/scheduler.h"
#include "evolution/sim/species_index_system.h"

namespace evolution::sim {

/**
 * @brief Configuration for generational evolution.
 */
struct EvolutionConfig {
    std::uint32_t population_size{100};
    std::uint32_t elite_count{5};
    double crossover_rate{0.75};
    double mutation_rate{1.0};
    std::uint32_t max_stagnation{15};
    std::size_t keep_minimum_species{2};
    bool interspecies_mating{false};
    double interspecies_mating_rate{0.001};
};

/**
 * @brief Statistics from an evolution generation.
 */
struct GenerationStats {
    std::uint32_t generation{0};
    std::size_t population_size{0};
    std::size_t species_count{0};
    double best_fitness{0.0};
    double mean_fitness{0.0};
    double best_adjusted_fitness{0.0};
    genetics::GenomeId champion_id{0};
};

/**
 * @brief Generational evolution system managing NEAT populations.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity Variable based on population size.
 * @note Implements standard NEAT generational evolution with speciation.
 * @warning Requires external fitness evaluation between generations.
 * @threadsafe @notthreadsafe.
 */
class EvolutionSystem final : public ISystem {
public:
    using FitnessFunction = std::function<double(genetics::GenomeId)>;

    /**
     * @brief Construct evolution system.
     * @param storage Genome storage reference.
     * @param innovations Innovation database reference.
     * @param species Species index system reference.
     * @param config Evolution configuration.
     * @param repro_config Reproduction configuration.
     * @param global_seed Random seed for determinism.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note All referenced systems must outlive EvolutionSystem.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    explicit EvolutionSystem(
        genetics::GenomeStorage& storage,
        genetics::InnovationDatabase& innovations,
        SpeciesIndexSystem& species,
        const EvolutionConfig& config,
        const genetics::ReproConfig& repro_config,
        std::uint64_t global_seed) noexcept;

    /**
     * @brief No-op tick (evolution is triggered explicitly).
     * @param context Simulation context.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Use advance_generation() to evolve.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Get system name.
     * @param None.
     * @return System identifier.
     * @throws None.
     * @complexity O(1).
     * @note None.
     * @warning None.
     * @threadsafe Thread-safe.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    /**
     * @brief Set fitness values for current population.
     * @param fitness_map Map of genome_id -> raw_fitness.
     * @return None.
     * @throws None.
     * @complexity O(N).
     * @note Must be called before advance_generation().
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    void set_fitness(const std::unordered_map<genetics::GenomeId, double>& fitness_map);

    /**
     * @brief Perform generational evolution.
     * @param None.
     * @return Statistics from completed generation.
     * @throws None.
     * @complexity O(N² * C) where N = population, C = genome complexity.
     * @note Evaluates fitness, selects parents, produces offspring, replaces population.
     * @warning set_fitness() must be called first.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] GenerationStats advance_generation();

    /**
     * @brief Initialize population with random genomes.
     * @param template_genome Template for initial population.
     * @param count Number of genomes to create.
     * @return None.
     * @throws None.
     * @complexity O(N * G) where G = genome size.
     * @note Clears existing population.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    void initialize_population(const evolution::genome::GenomeT& template_genome,
                                std::size_t count);

    /**
     * @brief Get current generation number.
     * @param None.
     * @return Generation counter.
     * @throws None.
     * @complexity O(1).
     * @note None.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::uint32_t generation() const noexcept { return generation_; }

    /**
     * @brief Get current population genome IDs.
     * @param None.
     * @return Vector of genome IDs.
     * @throws None.
     * @complexity O(1).
     * @note None.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] const std::vector<genetics::GenomeId>& population() const noexcept {
        return population_;
    }

    /**
     * @brief Get champion genome ID.
     * @param None.
     * @return ID of highest fitness genome.
     * @throws None.
     * @complexity O(1).
     * @note None.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] genetics::GenomeId champion_id() const noexcept { return champion_id_; }

private:
    struct RankedGenome {
        genetics::GenomeId id;
        double raw_fitness;
        double adjusted_fitness;
        SpeciesId species_id;
    };

    void ComputeAdjustedFitness();
    std::vector<genetics::GenomeId> SelectParents(std::size_t count);
    genetics::GenomeId SelectFromSpecies(const Species& species);
    genetics::GenomeId ProduceOffspring(genetics::GenomeId parent_a,
                                         genetics::GenomeId parent_b);
    void ReplacePopulation(const std::vector<genetics::GenomeId>& offspring);

    static constexpr std::string_view name_ = "evolution";
    
    genetics::GenomeStorage& storage_;
    genetics::InnovationDatabase& innovations_;
    SpeciesIndexSystem& species_system_;
    EvolutionConfig config_;
    genetics::ReproConfig repro_config_;
    
    std::uint32_t generation_{0};
    std::uint64_t global_seed_;
    std::vector<genetics::GenomeId> population_{};
    std::unordered_map<genetics::GenomeId, double> fitness_map_{};
    std::vector<RankedGenome> ranked_population_{};
    genetics::GenomeId champion_id_{0};
};

}  // namespace evolution::sim

