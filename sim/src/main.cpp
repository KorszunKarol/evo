/**
 * @file main.cpp
 * @brief Headless entry point that runs the default evolution scenario harness.
 */

#include <spdlog/spdlog.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/scenario.h"

int main() {
    using namespace evolution::sim;

    spdlog::set_level(spdlog::level::info);
    spdlog::info("Launching evolution scenario");

    SimulationApp app;
    genetics::GenomeStorage genome_storage;

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

    scenario.initial_population = 24;
    scenario.genome_seed = 0xDEADBEEF;
    scenario.reproduction_seed = 0xBADA55;

    setup_scenario(app, genome_storage, scenario);

    constexpr std::size_t kSteps = 600;
    spdlog::info("Executing scenario for {} steps", kSteps);
    app.run_for_steps(kSteps);
    spdlog::info("Scenario complete at t={}s", app.simulation_time());
    return 0;
}

