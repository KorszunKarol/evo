#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_ops.h"
#include "evolution/sim/adaptive_control_system.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/population_monitor.h"
#include "evolution/sim/simulation_app.h"
#include "evolution/sim/telemetry_system.h"

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
    bool enable_population_system{true};    ///< Run population monitoring/control system.
    PopulationConfig population{};          ///< Population dynamics and safety-net configuration.
    bool enable_telemetry{true};           ///< Enable telemetry system and outputs.
    std::string telemetry_output_dir{"output"}; ///< Output root for telemetry files.
    std::string telemetry_run_id{"default"};   ///< Telemetry run identifier.
    double telemetry_rollup_interval{2.0}; ///< Rollup cadence in seconds (0 disables).
    std::size_t telemetry_buffer_size{1000}; ///< Flush threshold for event buffer.
    double telemetry_sampling_rate{0.0};   ///< Sampling rate for non-targeted events.
    std::size_t telemetry_max_events_per_second{5000}; ///< Global event budget (0 disables).
    std::size_t telemetry_max_events_per_type_per_second{1000}; ///< Per-type budget (0 disables).
    MovementCaptureMode telemetry_movement_capture_mode{MovementCaptureMode::TargetedOnly}; ///< Movement telemetry mode.
    double species_index_interval_s{5.0};  ///< Cadence for expensive species clustering updates.
    double stats_interval_s{2.0};          ///< Stats emission interval in seconds.
    bool enable_feeding_debug{false};      ///< Enables periodic feeding diagnostic logs.
    double feeding_debug_interval_s{5.0};  ///< Feeding debug log interval in seconds.
    bool enable_species_info_logs{false};  ///< Enables species index info-level clustering logs.
    AdaptiveControlConfig adaptive_control{}; ///< Closed-loop ecological control tuning.
    double predation_damage_scale{1.0}; ///< Scalar for carnivore damage application.
    double predation_conversion_efficiency_scale{1.0}; ///< Scalar for carnivore harvest efficiency.
    double pursuit_timeout_scale{1.0}; ///< Scalar for pursuit timeout windows.
    double attack_cooldown_scale{1.0}; ///< Scalar for attack cooldown.
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
