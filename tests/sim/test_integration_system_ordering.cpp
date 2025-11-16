#include "test_fixtures.h"

#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/environment/feeding_system.h"
#include "evolution/sim/environment/plant_systems.h"
#include "evolution/sim/environment/soil_system.h"
#include "evolution/sim/fitness_update_system.h"
#include "evolution/sim/metabolism_system.h"
#include "evolution/sim/physics/simple_backend.h"
#include "evolution/sim/physics_system.h"
#include "evolution/sim/stats_system.h"

using namespace evolution::sim::test;
using namespace evolution::sim;
using namespace evolution::genetics;

TEST(IntegrationSystemOrdering, DeterministicRun) {
    SimulationFixture fixture1, fixture2;

    // Set up identical environments
    auto config1 = create_test_env_config(2025);
    auto config2 = create_test_env_config(2025);
    initialize_environment(fixture1.app().registry(), config1);
    initialize_environment(fixture2.app().registry(), config2);

    // Spawn identical entities
    const GenomeId genome_id1 = fixture1.create_test_genome(12345);
    const GenomeId genome_id2 = fixture2.create_test_genome(12345);

    fixture1.spawn_herbivore(Vec3{10, 0, 10}, genome_id1);
    fixture2.spawn_herbivore(Vec3{10, 0, 10}, genome_id2);

    fixture1.spawn_plant(Vec3{15, 0, 15}, 0, 10.0);
    fixture2.spawn_plant(Vec3{15, 0, 15}, 0, 10.0);

    // Register systems in same order
    fixture1.app().scheduler().add_system(std::make_unique<SoilSystem>());
    fixture1.app().scheduler().add_system(std::make_unique<PlantGrowthSystem>());
    fixture1.app().scheduler().add_system(std::make_unique<PlantSeedingSystem>(2026));
    fixture1.app().scheduler().add_system(std::make_unique<PlantSpatialSystem>());
    fixture1.app().scheduler().add_system(std::make_unique<FeedingSystem>());
    fixture1.app().scheduler().add_system(std::make_unique<PlantCleanupSystem>());
    fixture1.app().scheduler().add_system(std::make_unique<FitnessUpdateSystem>(FitnessWeights{1.0, 1.0, 1.0}));
    fixture1.app().scheduler().add_system(std::make_unique<MetabolismSystem>());

    fixture2.app().scheduler().add_system(std::make_unique<SoilSystem>());
    fixture2.app().scheduler().add_system(std::make_unique<PlantGrowthSystem>());
    fixture2.app().scheduler().add_system(std::make_unique<PlantSeedingSystem>(2026));
    fixture2.app().scheduler().add_system(std::make_unique<PlantSpatialSystem>());
    fixture2.app().scheduler().add_system(std::make_unique<FeedingSystem>());
    fixture2.app().scheduler().add_system(std::make_unique<PlantCleanupSystem>());
    fixture2.app().scheduler().add_system(std::make_unique<FitnessUpdateSystem>(FitnessWeights{1.0, 1.0, 1.0}));
    fixture2.app().scheduler().add_system(std::make_unique<MetabolismSystem>());

    // Run same number of steps
    fixture1.run_steps(100);
    fixture2.run_steps(100);

    // Take snapshots
    const auto snap1 = fixture1.take_snapshot();
    const auto snap2 = fixture2.take_snapshot();

    EXPECT_EQ(snap1.entity_count, snap2.entity_count);
    EXPECT_EQ(snap1.plant_count, snap2.plant_count);
    EXPECT_EQ(snap1.herbivore_count, snap2.herbivore_count);
    EXPECT_NEAR(snap1.total_biomass, snap2.total_biomass, 1e-3);
    EXPECT_EQ(snap1.state_hash, snap2.state_hash) << "State should be identical";
}

TEST(IntegrationSystemOrdering, SoilBeforePlantGrowth) {
    SimulationFixture fixture;
    auto config = create_test_env_config(2025);
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    auto& soil = registry.ctx().get<SoilGrid>();

    // Set soil to low value
    for (int x = 0; x < soil.width(); ++x) {
        for (int z = 0; z < soil.height(); ++z) {
            soil.at(x, z) = 1.0F;
        }
    }

    const entt::entity plant = fixture.spawn_plant(Vec3{50, 0, 50}, 0, 5.0);
    auto& plant_comp = registry.get<PlantComponent>(plant);
    const double initial_energy = plant_comp.energy;

    // Run soil then growth
    SoilSystem soil_system;
    PlantGrowthSystem growth_system;

    SimulationContext context(registry, 0.016, 0.0);
    soil_system.tick(context);
    growth_system.tick(context);

    EXPECT_GT(plant_comp.energy, initial_energy) << "Plant should grow after soil update";
}

TEST(IntegrationSystemOrdering, FeedingAfterPlantGrowth) {
    SimulationFixture fixture;
    auto config = create_test_env_config(2025);
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();

    const entt::entity plant = fixture.spawn_plant(Vec3{10, 0, 10}, 0, 20.0);
    const GenomeId genome_id = fixture.create_test_genome(12345);
    const entt::entity herbivore = fixture.spawn_herbivore(Vec3{10, 0, 10}, genome_id);

    // Rebuild PlantSpatialIndex before feeding (normally done by PlantSpatialSystem)
    auto* spatial_index = registry.ctx().find<PlantSpatialIndex>();
    if (spatial_index != nullptr) {
        spatial_index->rebuild(registry);
    }

    auto& plant_comp = registry.get<PlantComponent>(plant);
    auto& metab = registry.get<MetabolismComponent>(herbivore);
    // Phenotype builder sets energy to max_energy, so set it lower to allow feeding
    metab.energy = metab.max_energy * 0.5;
    const double initial_plant_energy = plant_comp.energy;
    const double initial_herb_energy = metab.energy;
    
    // Ensure plant is alive and has energy
    EXPECT_TRUE(plant_comp.alive);
    EXPECT_GT(plant_comp.energy, 0.0);
    // Ensure herbivore has capacity and wants to eat
    EXPECT_LT(metab.energy, metab.max_energy);
    const auto& intent = registry.get<FeedingIntent>(herbivore);
    EXPECT_TRUE(intent.request_eat);

    FeedingSystem feeding_system;
    SimulationContext context(registry, 0.016, 0.0);
    feeding_system.tick(context);

    EXPECT_LT(plant_comp.energy, initial_plant_energy) << "Plant should lose energy";
    EXPECT_GT(metab.energy, initial_herb_energy) << "Herbivore should gain energy";
}

TEST(IntegrationSystemOrdering, FitnessAfterFeeding) {
    SimulationFixture fixture;
    auto config = create_test_env_config(2025);
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();

    const GenomeId genome_id = fixture.create_test_genome(12345);
    const entt::entity herbivore = fixture.spawn_herbivore(Vec3{10, 0, 10}, genome_id);

    auto& metab = registry.get<MetabolismComponent>(herbivore);
    metab.energy = 50.0;

    auto& fitness = registry.get<FitnessComponent>(herbivore);
    const double initial_energy_int = fitness.energy_int_accum;

    FitnessUpdateSystem fitness_system(FitnessWeights{0.0, 1.0, 0.0});  // Only energy weight

    SimulationContext context(registry, 0.016, 0.0);
    fitness_system.tick(context);

    EXPECT_GT(fitness.energy_int_accum, initial_energy_int)
        << "Fitness should accumulate energy integral";
}

TEST(IntegrationSystemOrdering, MetabolismAfterFitness) {
    SimulationFixture fixture;
    auto config = create_test_env_config(2025);
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();

    const GenomeId genome_id = fixture.create_test_genome(12345);
    const entt::entity herbivore = fixture.spawn_herbivore(Vec3{10, 0, 10}, genome_id);

    auto& metab = registry.get<MetabolismComponent>(herbivore);
    metab.energy = 100.0;
    metab.basal_rate = 1.0;

    FitnessUpdateSystem fitness_system(FitnessWeights{1.0, 1.0, 1.0});
    MetabolismSystem metabolism_system(false);  // Don't destroy on zero

    SimulationContext context(registry, 0.016, 0.0);
    fitness_system.tick(context);
    metabolism_system.tick(context);

    EXPECT_LT(metab.energy, 100.0) << "Metabolism should consume energy";
}

TEST(IntegrationSystemOrdering, PlantCleanupAfterDepletion) {
    SimulationFixture fixture;
    auto config = create_test_env_config(2025);
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();

    const entt::entity plant = fixture.spawn_plant(Vec3{10, 0, 10}, 0, 0.1);
    auto& plant_comp = registry.get<PlantComponent>(plant);
    plant_comp.alive = false;
    plant_comp.time_since_depleted = 15.0;  // Past cleanup delay

    const std::size_t initial_count = registry.storage<entt::entity>().in_use();

    PlantCleanupSystem cleanup_system;
    SimulationContext context(registry, 0.016, 0.0);
    cleanup_system.tick(context);

    EXPECT_LT(registry.storage<entt::entity>().in_use(), initial_count)
        << "Dead plants should be cleaned up";
}

TEST(IntegrationSystemOrdering, MultipleRunsDeterministic) {
    SimulationFixture fixture;
    auto config = create_test_env_config(2025);
    initialize_environment(fixture.app().registry(), config);

    fixture.app().scheduler().add_system(std::make_unique<SoilSystem>());
    fixture.app().scheduler().add_system(std::make_unique<PlantGrowthSystem>());
    fixture.app().scheduler().add_system(std::make_unique<FitnessUpdateSystem>(FitnessWeights{1.0, 1.0, 1.0}));
    fixture.app().scheduler().add_system(std::make_unique<MetabolismSystem>());

    // Run first batch
    fixture.run_steps(50);
    const auto snap1 = fixture.take_snapshot();

    // Reset and run again
    SimulationFixture fixture2;
    initialize_environment(fixture2.app().registry(), config);
    fixture2.app().scheduler().add_system(std::make_unique<SoilSystem>());
    fixture2.app().scheduler().add_system(std::make_unique<PlantGrowthSystem>());
    fixture2.app().scheduler().add_system(std::make_unique<FitnessUpdateSystem>(FitnessWeights{1.0, 1.0, 1.0}));
    fixture2.app().scheduler().add_system(std::make_unique<MetabolismSystem>());

    fixture2.run_steps(50);
    const auto snap2 = fixture2.take_snapshot();

    EXPECT_EQ(snap1.state_hash, snap2.state_hash) << "Multiple runs should be deterministic";
}

