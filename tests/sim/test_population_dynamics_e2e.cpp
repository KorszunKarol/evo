#include <vector>

#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/population_monitor.h"
#include "evolution/sim/scenario.h"
#include "evolution/sim/simulation_app.h"

using namespace evolution::sim;

namespace {

SimulationScenario MakeScenario() {
    SimulationScenario scenario{};
    scenario.enable_telemetry = false;
    scenario.enable_species_index = false;
    scenario.enable_population_system = true;

    scenario.environment.terrain.width_cells = 64;
    scenario.environment.terrain.height_cells = 64;
    scenario.environment.terrain.cell_size = 1.5;
    scenario.environment.terrain.seed = 2026;

    scenario.environment.soil.width_cells = 32;
    scenario.environment.soil.height_cells = 32;
    scenario.environment.soil.cell_size = 3.0;
    scenario.environment.plants.initial_count = 120;
    scenario.environment.plants.seed = 2027;
    scenario.environment.plant_spatial_cell_size = 3.0;

    scenario.initial_population = 24;
    scenario.genome_seed = 9090;
    scenario.reproduction_seed = 9091;

    scenario.population.min_viable_population = 6;
    scenario.population.extinction_grace_ticks = 60;
    scenario.population.rescue_batch_size = 6;
    scenario.population.max_carry_capacity = 120;
    scenario.population.hard_cap = 180;
    scenario.population.target_cap = 140;
    scenario.population.world_area_override = 0.0;

    return scenario;
}

}  // namespace

TEST(PopulationDynamicsE2E, PopulationStaysWithinHardCap) {
    SimulationApp app;
    evolution::genetics::GenomeStorage storage;
    auto scenario = MakeScenario();

    setup_scenario(app, storage, scenario);

    for (int i = 0; i < 500; ++i) {
        app.tick();
        const auto* monitor = app.registry().ctx().find<PopulationMonitor>();
        ASSERT_NE(monitor, nullptr);
        EXPECT_LE(monitor->latest.creature_count, scenario.population.hard_cap);
    }
}

TEST(PopulationDynamicsE2E, RescueTriggersWhenBelowMinViable) {
    SimulationApp app;
    evolution::genetics::GenomeStorage storage;
    auto scenario = MakeScenario();
    scenario.initial_population = 1;
    scenario.population.min_viable_population = 8;
    scenario.population.extinction_grace_ticks = 2;
    scenario.population.rescue_batch_size = 4;

    setup_scenario(app, storage, scenario);

    for (int i = 0; i < 10; ++i) {
        app.tick();
    }

    const auto* counters = app.registry().ctx().find<PopulationEventCounters>();
    ASSERT_NE(counters, nullptr);
    EXPECT_GE(counters->rescues_total, 1u);
}

TEST(PopulationDynamicsE2E, DeterministicPopulationTrajectory) {
    auto run_trajectory = []() {
        SimulationApp app;
        evolution::genetics::GenomeStorage storage;
        auto scenario = MakeScenario();
        setup_scenario(app, storage, scenario);

        std::vector<std::size_t> trajectory;
        trajectory.reserve(240);
        for (int i = 0; i < 240; ++i) {
            app.tick();
            const auto* monitor = app.registry().ctx().find<PopulationMonitor>();
            trajectory.push_back(monitor != nullptr ? monitor->latest.creature_count : 0u);
        }
        return trajectory;
    };

    const auto a = run_trajectory();
    const auto b = run_trajectory();
    EXPECT_EQ(a, b);
}
