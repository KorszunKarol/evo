#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_ops.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/simulation_app.h"

namespace evolution::sim {

/**
 * @brief High-level configuration for running an evolution experiment.
 */
struct SimulationScenario {
    EnvironmentConfig environment{};        ///< Terrain/soil/plants configuration.
    genetics::ReproConfig reproduction{};   ///< Reproduction and mutation parameters.
    std::size_t initial_population{24};     ///< Number of randomly generated creatures at start.
    std::uint64_t genome_seed{2025};        ///< Seed for initial genome generation.
    std::uint64_t reproduction_seed{0xBEEFu}; ///< Global reproduction system seed.
    bool enable_species_index{true};        ///< Run species clustering system.
    bool enable_telemetry{true};           ///< Enable telemetry system and outputs.
    std::string telemetry_output_dir{"output"}; ///< Output root for telemetry files.
    std::string telemetry_run_id{"default"};   ///< Telemetry run identifier.
    double telemetry_rollup_interval{1.0}; ///< Rollup cadence in seconds (0 disables).
    std::size_t telemetry_buffer_size{1000}; ///< Flush threshold for event buffer.
    double telemetry_sampling_rate{0.0};   ///< Sampling rate for non-targeted events.
};

/**
 * @brief Initializes the simulation app according to the supplied scenario.
 *
 * @param app Simulation application to configure.
 * @param storage Genome storage that outlives registered systems.
 * @param scenario Scenario configuration (environment + evolution settings).
 */
void setup_scenario(SimulationApp& app,
                    genetics::GenomeStorage& storage,
                    const SimulationScenario& scenario);

/**
 * @brief Seeds an initial population of randomly generated genomes.
 *
 * @param registry Registry receiving the entities.
 * @param storage Genome storage used to create genomes.
 * @param scenario Scenario configuration for spawn counts.
 */
void seed_initial_population(entt::registry& registry,
                             genetics::GenomeStorage& storage,
                             const SimulationScenario& scenario);

}  // namespace evolution::sim


