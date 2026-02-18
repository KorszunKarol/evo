/**
 * @file main.cpp
 * @brief Headless entry point that runs the default evolution scenario harness.
 */

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/scenario.h"

int main() {
    using namespace evolution::sim;

    spdlog::set_level(spdlog::level::info);
    spdlog::info("Launching evolution scenario");

    SimulationApp app;
    evolution::genetics::GenomeStorage genome_storage;

    SimulationScenario scenario{};
    scenario.environment.terrain.width_cells = 512;
    scenario.environment.terrain.height_cells = 512;
    scenario.environment.terrain.cell_size = 2.0;
    scenario.environment.terrain.elevation_scale = 18.0;
    scenario.environment.terrain.octaves = 5;
    scenario.environment.terrain.base_frequency = 0.004;
    scenario.environment.terrain.seed = 2025;

    scenario.environment.soil.width_cells = 256;
    scenario.environment.soil.height_cells = 256;
    scenario.environment.soil.cell_size = 4.0;
    scenario.environment.soil.max_nutrient = 12.0F;
    scenario.environment.soil.diffusion_rate = 0.45F;
    scenario.environment.soil.regeneration_rate = 0.08F;
    scenario.environment.soil.baseline_nutrient = 5.0F;

    scenario.environment.plants.initial_count = 800;
    scenario.environment.plants.min_initial_energy = 6.0;
    scenario.environment.plants.max_initial_energy = 16.0;
    scenario.environment.plants.seed = 1337;
    scenario.environment.plant_spatial_cell_size = 4.0;
    scenario.population.plant_soft_capacity = 6000;
    scenario.population.plant_high_pressure_capacity = 12000;
    scenario.population.min_plant_seeding_multiplier = 0.02;
    scenario.population.max_plant_seeding_multiplier = 1.0;
    scenario.population.plant_pressure_gain = 1.0;
    scenario.population.density_feedback_gain = 1.2;
    scenario.population.herbivore_pressure_gain = 0.8;
    scenario.population.seeding_update_interval_s = 1.0;
    scenario.population.hard_cap = 2000;
    scenario.population.target_cap = 1500;
    scenario.population.max_carry_capacity = 1200;

    scenario.initial_population = 24;
    scenario.genome_seed = 0xDEADBEEF;
    scenario.reproduction_seed = 0xBADA55;
    scenario.species_index_interval_s = 5.0;
    scenario.stats_interval_s = 2.0;
    scenario.enable_feeding_debug = false;
    scenario.feeding_debug_interval_s = 5.0;
    scenario.enable_species_info_logs = false;
    scenario.telemetry_rollup_interval = 2.0;
    scenario.telemetry_max_events_per_second = 4000;
    scenario.telemetry_max_events_per_type_per_second = 800;
    scenario.telemetry_movement_capture_mode = MovementCaptureMode::TargetedOnly;
    scenario.adaptive_control.enabled = true;
    scenario.adaptive_control.target_population_min = 300;
    scenario.adaptive_control.target_population_max = 1200;
    scenario.adaptive_control.target_herbivore_plant_ratio_min = 0.03;
    scenario.adaptive_control.target_herbivore_plant_ratio_max = 0.25;
    scenario.adaptive_control.max_control_step_per_sec = 0.08;
    scenario.adaptive_control.cooldown_after_rescue_s = 20.0;
    scenario.predation_damage_scale = 1.0;
    scenario.predation_conversion_efficiency_scale = 1.0;
    scenario.pursuit_timeout_scale = 1.0;
    scenario.attack_cooldown_scale = 1.0;

    const std::string preset = std::getenv("SIM_PRESET") != nullptr ? std::getenv("SIM_PRESET") : "default";
    if (const char* out_dir = std::getenv("SIM_OUTPUT_DIR"); out_dir != nullptr && *out_dir != '\0') {
        scenario.telemetry_output_dir = out_dir;
    }
    if (preset == "stability_low_resources") {
        scenario.environment.soil.regeneration_rate = 0.04F;
        scenario.environment.soil.baseline_nutrient = 3.0F;
        scenario.environment.plants.initial_count = 500;
    } else if (preset == "stability_high_density") {
        scenario.initial_population = 180;
        scenario.population.hard_cap = 2600;
        scenario.population.target_cap = 1800;
        scenario.population.max_carry_capacity = 1500;
        scenario.environment.plants.initial_count = 1400;
    } else if (preset == "stability_predator_pressure") {
        scenario.predation_damage_scale = 1.25;
        scenario.predation_conversion_efficiency_scale = 0.85;
        scenario.attack_cooldown_scale = 0.85;
    }

    setup_scenario(app, genome_storage, scenario);

    std::size_t kSteps = 10800;
    if (const char* steps_env = std::getenv("SIM_STEPS"); steps_env != nullptr && *steps_env != '\0') {
        try {
            kSteps = static_cast<std::size_t>(std::stoull(steps_env));
        } catch (...) {
            spdlog::warn("Invalid SIM_STEPS value '{}', using default {}", steps_env, kSteps);
        }
    }
    spdlog::info("Using preset '{}'", preset);
    spdlog::info("Executing scenario for {} steps", kSteps);
    app.run_for_steps(kSteps);
    spdlog::info("Scenario complete at t={}s", app.simulation_time());
    return 0;
}
