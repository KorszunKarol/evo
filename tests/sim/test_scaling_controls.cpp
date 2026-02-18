#include <cstdint>

#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/environment/plant_systems.h"
#include "evolution/sim/population_monitor.h"
#include "evolution/sim/simulation_context.h"
#include "evolution/sim/species_index_system.h"
#include "test_fixtures.h"

using namespace evolution::sim;

TEST(SpeciesIndexCadence, RunsOnlyWhenIntervalElapses) {
    evolution::genetics::GenomeStorage storage;
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        storage.create_random(seed);
    }

    evolution::genetics::ReproConfig config{};
    SpeciesIndexSystem system(storage, config, 10, 3.0, 2.0, false);
    entt::registry registry;

    SimulationContext ctx0(registry, 0.5, 0.0);
    system.tick(ctx0);
    EXPECT_EQ(system.species_count(), 0u);

    SimulationContext ctx1(registry, 0.5, 0.5);
    system.tick(ctx1);
    EXPECT_EQ(system.species_count(), 0u);

    SimulationContext ctx2(registry, 0.5, 1.0);
    system.tick(ctx2);
    EXPECT_EQ(system.species_count(), 0u);

    SimulationContext ctx3(registry, 0.5, 1.5);
    system.tick(ctx3);
    EXPECT_GT(system.species_count(), 0u);
}

TEST(PlantSeedingScaling, HerbivorePressureRelaxesPlantThrottle) {
    entt::registry registry;
    EnvironmentConfig config = evolution::sim::test::create_test_env_config(2026);
    initialize_environment(registry, config);
    const auto& terrain = registry.ctx().get<Terrain>();

    for (int i = 0; i < 30; ++i) {
        const entt::entity plant_entity = registry.create();
        const double x = 2.0 + static_cast<double>(i) * 2.0;
        const double z = 4.0 + static_cast<double>((i * 3) % 20) * 2.0;
        registry.emplace<TransformComponent>(
            plant_entity, TransformComponent{.position = Vec3{x, terrain.height(x, z), z}});

        PlantComponent plant{};
        plant.alive = true;
        plant.energy = 20.0;
        plant.max_energy = 20.0;
        plant.seed_timer = 100.0;
        plant.seed_interval = 1.0;
        registry.emplace<PlantComponent>(plant_entity, plant);

        PlantSeedParams seed_params{};
        seed_params.seed_min_energy = 1.0;
        seed_params.seed_cost = 0.1;
        seed_params.seed_radius = 1.0;
        seed_params.establish_probability = 1.0;
        registry.emplace<PlantSeedParams>(plant_entity, seed_params);
    }

    PopulationMonitor monitor{};
    monitor.config.plant_soft_capacity = 10;
    monitor.config.plant_high_pressure_capacity = 20;
    monitor.config.plant_pressure_gain = 1.0;
    monitor.config.min_plant_seeding_multiplier = 0.02;
    monitor.config.max_plant_seeding_multiplier = 1.0;
    monitor.config.density_feedback_gain = 1.2;
    monitor.config.herbivore_pressure_gain = 0.8;
    monitor.config.seeding_update_interval_s = 0.0;
    monitor.latest.herbivore_count = 0;
    registry.ctx().emplace<PopulationMonitor>(monitor);

    PlantSeedingSystem system(PlantSeedingSystem::Tuning{.seed = 77, .update_interval_s = 0.0});
    SimulationContext ctx_low(registry, 1.0 / 60.0, 0.0);
    system.tick(ctx_low);
    const double low_pressure_multiplier = system.debug_seeding_multiplier();
    EXPECT_LT(low_pressure_multiplier, 0.30);

    auto& monitor_ref = registry.ctx().get<PopulationMonitor>();
    monitor_ref.latest.herbivore_count = 40;
    SimulationContext ctx_high(registry, 1.0 / 60.0, 1.0);
    system.tick(ctx_high);
    const double high_pressure_multiplier = system.debug_seeding_multiplier();
    EXPECT_GT(high_pressure_multiplier, low_pressure_multiplier);
}
