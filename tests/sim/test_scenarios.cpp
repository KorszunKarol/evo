#include "test_fixtures.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/environment/feeding_system.h"
#include "evolution/sim/environment/plant_systems.h"
#include "evolution/sim/environment/soil_system.h"
#include "evolution/sim/fitness_update_system.h"
#include "evolution/sim/metabolism_system.h"
#include "evolution/sim/reproduction_system.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace evolution::sim::test;
using namespace evolution::sim;

class ScenarioFixture final : public SimulationFixture {
public:
    void Initialize() {
        SetUp();
        auto& scheduler = app().scheduler();
        scheduler.add_system(std::make_unique<SoilSystem>());
        scheduler.add_system(std::make_unique<PlantSeedingSystem>(2026));
        scheduler.add_system(std::make_unique<PlantSpatialSystem>());
        scheduler.add_system(std::make_unique<FeedingSystem>());
        scheduler.add_system(std::make_unique<PlantCleanupSystem>());
        scheduler.add_system(std::make_unique<FitnessUpdateSystem>(FitnessWeights{1.0, 1.0, 1.0}));
        scheduler.add_system(std::make_unique<MetabolismSystem>());
    }

    [[nodiscard]] entt::entity spawn_herbivore(const Vec3& position,
                                               evolution::genetics::GenomeId genome_id) {
        const entt::entity entity = SimulationFixture::spawn_herbivore(position, genome_id);
        auto& registry = app().registry();
        auto& metabolism = registry.get<MetabolismComponent>(entity);
        metabolism.basal_rate = 40.0;
        auto& intent = registry.get<FeedingIntent>(entity);
        intent.rate = 4.0;
        return entity;
    }
};

class ScenarioTests : public ::testing::Test {
protected:
    ScenarioFixture fixture;
};

// 1. Basic Reproduction Cycle
TEST_F(ScenarioTests, BasicReproductionCycle) {
    fixture.Initialize();
    
    // Create initial herbivores
    auto genome1 = fixture.create_test_genome(1001);
    auto genome2 = fixture.create_test_genome(1002);
    auto h1 = fixture.spawn_herbivore({1.0, 0.0, 1.0}, genome1);
    auto h2 = fixture.spawn_herbivore({3.0, 0.0, 3.0}, genome2);
    
    // Give enough energy to reproduce
    auto& registry = fixture.app().registry();
    auto& metab1 = registry.get<MetabolismComponent>(h1);
    auto& metab2 = registry.get<MetabolismComponent>(h2);
    metab1.energy = 150.0;
    metab2.energy = 150.0;

    auto& repro1 = registry.get<ReproductionComponent>(h1);
    auto& repro2 = registry.get<ReproductionComponent>(h2);
    repro1.timer = 0.0;
    repro2.timer = 0.0;
    repro1.energy_threshold = 50.0;
    repro2.energy_threshold = 50.0;
    repro1.mate_radius = 0.0;
    repro2.mate_radius = 0.0;

    ReproductionSystem repro_system(fixture.storage(), evolution::genetics::ReproConfig{}, 12345);
    repro_system.set_asexual_fallback(true);
    SimulationContext repro_context(registry, 0.016, 0.0);
    repro_system.tick(repro_context);
    
    fixture.run_steps(20);
    
    // Validate offspring produced
    auto snap = fixture.take_snapshot();
    EXPECT_GT(snap.herbivore_count, 2);
    EXPECT_GT(snap.species_counts.size(), 0);
    
    // Test determinism
    ScenarioFixture fixture2;
    fixture2.Initialize();
    auto genome1b = fixture2.create_test_genome(1001);
    auto genome2b = fixture2.create_test_genome(1002);
    auto h1b = fixture2.spawn_herbivore({1.0, 0.0, 1.0}, genome1b);
    auto h2b = fixture2.spawn_herbivore({3.0, 0.0, 3.0}, genome2b);
    auto& registry2 = fixture2.app().registry();
    auto& metab1b = registry2.get<MetabolismComponent>(h1b);
    auto& metab2b = registry2.get<MetabolismComponent>(h2b);
    metab1b.energy = 150.0;
    metab2b.energy = 150.0;

    auto& repro1b = registry2.get<ReproductionComponent>(h1b);
    auto& repro2b = registry2.get<ReproductionComponent>(h2b);
    repro1b.timer = 0.0;
    repro2b.timer = 0.0;
    repro1b.energy_threshold = 50.0;
    repro2b.energy_threshold = 50.0;
    repro1b.mate_radius = 0.0;
    repro2b.mate_radius = 0.0;

    ReproductionSystem repro_system2(fixture2.storage(), evolution::genetics::ReproConfig{}, 12345);
    repro_system2.set_asexual_fallback(true);
    SimulationContext repro_context2(registry2, 0.016, 0.0);
    repro_system2.tick(repro_context2);
    
    fixture2.run_steps(20);
    auto snap2 = fixture2.take_snapshot();
    EXPECT_EQ(snap.state_hash, snap2.state_hash);
}

// 2. Predation Dynamics
TEST_F(ScenarioTests, PredationDynamics) {
    fixture.Initialize();
    
    auto genome = fixture.create_test_genome(2001);
    auto herbivore = fixture.spawn_herbivore({0.0, 0.0, 0.0}, genome);
    
    for (int i = 0; i < 5; ++i) {
        double angle = i * 6.28 / 5.0;
        double radius = 2.0;
        fixture.spawn_plant({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, 0, 15.0);
    }
    
    auto& registry = fixture.app().registry();
    auto& metab = registry.get<MetabolismComponent>(herbivore);
    metab.energy = 100.0;
    metab.basal_rate = 2.0;
    auto& intent = registry.get<FeedingIntent>(herbivore);
    intent.rate = 8.0;
    
    fixture.run_steps(10);
    auto snap = fixture.take_snapshot();
    
    auto metab_after = registry.get<MetabolismComponent>(herbivore);
    EXPECT_GT(metab_after.energy, 100.0);
    EXPECT_LT(snap.total_biomass, 5.0 * 15.0);
    
    ScenarioFixture fixture2;
    fixture2.Initialize();
    auto genome2 = fixture2.create_test_genome(2001);
    auto herbivore2 = fixture2.spawn_herbivore({0.0, 0.0, 0.0}, genome2);
    for (int i = 0; i < 5; ++i) {
        double angle = i * 6.28 / 5.0;
        double radius = 2.0;
        fixture2.spawn_plant({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, 0, 15.0);
    }
    auto& registry2 = fixture2.app().registry();
    auto& metab2 = registry2.get<MetabolismComponent>(herbivore2);
    metab2.energy = 100.0;
    metab2.basal_rate = 2.0;
    auto& intent2 = registry2.get<FeedingIntent>(herbivore2);
    intent2.rate = 8.0;
    fixture2.run_steps(10);
    auto snap2 = fixture2.take_snapshot();
    EXPECT_EQ(snap.state_hash, snap2.state_hash);
}

// 3. Species Formation
TEST_F(ScenarioTests, SpeciesFormation) {
    fixture.Initialize();
    
    std::vector<evolution::genetics::GenomeId> genomes;
    for (int i = 0; i < 10; ++i) {
        genomes.push_back(fixture.create_test_genome(3000 + i));
    }
    
    for (int i = 0; i < 10; ++i) {
        double angle = i * 6.28 / 10.0;
        double radius = 5.0;
        fixture.spawn_herbivore({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, genomes[i]);
    }
    
    fixture.run_steps(100);
    
    auto snap = fixture.take_snapshot();
    EXPECT_GT(snap.species_counts.size(), 1);
    EXPECT_LT(snap.species_counts.size(), 10);
    
    ScenarioFixture fixture2;
    fixture2.Initialize();
    std::vector<evolution::genetics::GenomeId> genomes2;
    for (int i = 0; i < 10; ++i) {
        genomes2.push_back(fixture2.create_test_genome(3000 + i));
    }
    for (int i = 0; i < 10; ++i) {
        double angle = i * 6.28 / 10.0;
        double radius = 5.0;
        fixture2.spawn_herbivore({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, genomes2[i]);
    }
    fixture2.run_steps(100);
    auto snap2 = fixture2.take_snapshot();
    EXPECT_EQ(snap.state_hash, snap2.state_hash);
}

// 4. Resource Depletion
TEST_F(ScenarioTests, ResourceDepletion) {
    fixture.Initialize();
    
    auto genome = fixture.create_test_genome(4000);
    for (int i = 0; i < 20; ++i) {
        double angle = i * 6.28 / 20.0;
        double radius = 4.0;
        fixture.spawn_herbivore({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, genome);
    }
    
    for (int i = 0; i < 50; ++i) {
        double x = (i % 10) * 6.0 - 3.0;
        double z = (i / 10) * 6.0 - 3.0;
        fixture.spawn_plant({x, 0.0, z}, 0, 5.0);
    }
    
    fixture.run_steps(500);
    
    auto snap = fixture.take_snapshot();
    EXPECT_LT(snap.total_biomass, 50.0 * 20.0);
    EXPECT_LT(snap.herbivore_count, 20);
    
    ScenarioFixture fixture2;
    fixture2.Initialize();
    auto genome2 = fixture2.create_test_genome(4000);
    for (int i = 0; i < 20; ++i) {
        double angle = i * 6.28 / 20.0;
        double radius = 4.0;
        fixture2.spawn_herbivore({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, genome2);
    }
    for (int i = 0; i < 50; ++i) {
        double x = (i % 10) * 6.0 - 3.0;
        double z = (i / 10) * 6.0 - 3.0;
        fixture2.spawn_plant({x, 0.0, z}, 0, 5.0);
    }
    fixture2.run_steps(500);
    auto snap2 = fixture2.take_snapshot();
    EXPECT_EQ(snap.state_hash, snap2.state_hash);
}

// 5. Extinction Event
TEST_F(ScenarioTests, ExtinctionEvent) {
    fixture.Initialize();
    
    auto genome = fixture.create_test_genome(5000);
    for (int i = 0; i < 10; ++i) {
        double angle = i * 6.28 / 10.0;
        double radius = 3.0;
        fixture.spawn_herbivore({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, genome);
    }

    {
        auto& registry = fixture.app().registry();
        auto view = registry.view<MetabolismComponent, FeedingIntent, HerbivoreTag>();
        view.each([](MetabolismComponent& metab, FeedingIntent& intent) {
            metab.basal_rate = 80.0;
            intent.rate = 2.0;
        });
    }
    
    for (int i = 0; i < 10; ++i) {
        double x = (i % 5) * 10.0 - 5.0;
        double z = (i / 5) * 10.0 - 5.0;
        fixture.spawn_plant({x, 0.0, z}, 0, 2.0);
    }
    
    fixture.run_steps(200);
    
    auto snap = fixture.take_snapshot();
    EXPECT_LT(snap.herbivore_count, 10);
    
    ScenarioFixture fixture2;
    fixture2.Initialize();
    auto genome2 = fixture2.create_test_genome(5000);
    for (int i = 0; i < 10; ++i) {
        double angle = i * 6.28 / 10.0;
        double radius = 3.0;
        fixture2.spawn_herbivore({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, genome2);
    }

    {
        auto& registry2 = fixture2.app().registry();
        auto view2 = registry2.view<MetabolismComponent, FeedingIntent, HerbivoreTag>();
        view2.each([](MetabolismComponent& metab, FeedingIntent& intent) {
            metab.basal_rate = 80.0;
            intent.rate = 2.0;
        });
    }
    for (int i = 0; i < 10; ++i) {
        double x = (i % 5) * 10.0 - 5.0;
        double z = (i / 5) * 10.0 - 5.0;
        fixture2.spawn_plant({x, 0.0, z}, 0, 2.0);
    }
    fixture2.run_steps(200);
    auto snap2 = fixture2.take_snapshot();
    EXPECT_EQ(snap.state_hash, snap2.state_hash);
}
