#include "test_fixtures.h"
#include "evolution/genetics/genome_storage.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace evolution::sim::test;

class ScenarioTests : public ::testing::Test {
protected:
    SimulationFixture fixture;
};

// 1. Basic Reproduction Cycle
TEST_F(ScenarioTests, BasicReproductionCycle) {
    fixture.SetUp();
    
    // Create initial herbivores
    auto genome1 = fixture.create_test_genome(1001);
    auto genome2 = fixture.create_test_genome(1002);
    auto h1 = fixture.spawn_herbivore({1.0, 0.0, 1.0}, genome1);
    auto h2 = fixture.spawn_herbivore({3.0, 0.0, 3.0}, genome2);
    
    // Give enough energy to reproduce
    auto& registry = fixture.app().registry();
    auto metab1 = registry.get<MetabolismComponent>(h1);
    auto metab2 = registry.get<MetabolismComponent>(h2);
    metab1.energy = 150.0;
    metab2.energy = 150.0;
    
    fixture.run_steps(20);
    
    // Validate offspring produced
    auto snap = fixture.take_snapshot();
    EXPECT_GT(snap.herbivore_count, 2);
    EXPECT_GT(snap.species_counts.size(), 0);
    
    // Test determinism
    SimulationFixture fixture2;
    fixture2.SetUp();
    auto genome1b = fixture2.create_test_genome(1001);
    auto genome2b = fixture2.create_test_genome(1002);
    auto h1b = fixture2.spawn_herbivore({1.0, 0.0, 1.0}, genome1b);
    auto h2b = fixture2.spawn_herbivore({3.0, 0.0, 3.0}, genome2b);
    auto& registry2 = fixture2.app().registry();
    auto metab1b = registry2.get<MetabolismComponent>(h1b);
    auto metab2b = registry2.get<MetabolismComponent>(h2b);
    metab1b.energy = 150.0;
    metab2b.energy = 150.0;
    
    fixture2.run_steps(20);
    auto snap2 = fixture2.take_snapshot();
    EXPECT_EQ(snap.state_hash, snap2.state_hash);
}

// 2. Predation Dynamics
TEST_F(ScenarioTests, PredationDynamics) {
    fixture.SetUp();
    
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
    auto metab = registry.get<MetabolismComponent>(herbivore);
    metab.energy = 100.0;
    
    fixture.run_steps(10);
    auto snap = fixture.take_snapshot();
    
    auto metab_after = registry.get<MetabolismComponent>(herbivore);
    EXPECT_GT(metab_after.energy, 100.0);
    EXPECT_LT(snap.total_biomass, 5.0 * 15.0);
    
    SimulationFixture fixture2;
    fixture2.SetUp();
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
    auto metab2 = registry2.get<MetabolismComponent>(herbivore2);
    metab2.energy = 100.0;
    fixture2.run_steps(10);
    auto snap2 = fixture2.take_snapshot();
    EXPECT_EQ(snap.state_hash, snap2.state_hash);
}

// 3. Species Formation
TEST_F(ScenarioTests, SpeciesFormation) {
    fixture.SetUp();
    
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
    
    SimulationFixture fixture2;
    fixture2.SetUp();
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
    fixture.SetUp();
    
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
    
    SimulationFixture fixture2;
    fixture2.SetUp();
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
    fixture.SetUp();
    
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
    
    for (int i = 0; i < 10; ++i) {
        double x = (i % 5) * 10.0 - 5.0;
        double z = (i / 5) * 10.0 - 5.0;
        fixture.spawn_plant({x, 0.0, z}, 0, 2.0);
    }
    
    fixture.run_steps(200);
    
    auto snap = fixture.take_snapshot();
    EXPECT_LT(snap.herbivore_count, 10);
    
    SimulationFixture fixture2;
    fixture2.SetUp();
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
    for (int i = 0; i < 10; ++i) {
        double x = (i % 5) * 10.0 - 5.0;
        double z = (i / 5) * 10.0 - 5.0;
        fixture2.spawn_plant({x, 0.0, z}, 0, 2.0);
    }
    fixture2.run_steps(200);
    auto snap2 = fixture2.take_snapshot();
    EXPECT_EQ(snap.state_hash, snap2.state_hash);
}
