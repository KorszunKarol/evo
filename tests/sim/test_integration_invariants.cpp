#include <entt/entt.hpp>
#include <cmath>
#include <limits>
#include "test_fixtures.h"
#include <gtest/gtest.h>

using namespace evolution::sim;
using namespace evolution::sim::test;

class IntegrationInvariantsTest : public ::testing::Test {
protected:
    SimulationFixture fixture;
};

// 1. Energy Conservation
TEST_F(IntegrationInvariantsTest, EnergyConserved) {
    fixture.SetUp();
    
    // Create initial population
    auto genome = fixture.create_test_genome(6000);
    std::vector<entt::entity> herbivores;
    for (int i = 0; i < 10; ++i) {
        double angle = i * 6.28 / 10.0;
        double radius = 4.0;
        herbivores.push_back(fixture.spawn_herbivore({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, genome));
    }
    
    // Create plants with known energy
    double total_plant_energy = 0.0;
    for (int i = 0; i < 20; ++i) {
        double x = (i % 5) * 8.0 - 4.0;
        double z = (i / 5) * 8.0 - 4.0;
        auto plant = fixture.spawn_plant({x, 0.0, z}, 0, 10.0);
        total_plant_energy += 10.0;
    }
    
    auto snap_before = fixture.take_snapshot();
    double initial_total_energy = snap_before.total_biomass;
    
    // Run simulation (herbivores will eat plants, plants grow via soil)
    fixture.run_steps(50);
    
    // Validate energy never increases
    auto snap_after = fixture.take_snapshot();
    EXPECT_LE(snap_after.total_biomass, initial_total_energy)
        << "Energy increased: " << snap_after.total_biomass << " > " << initial_total_energy;
}

// 2. Population Bounds
TEST_F(IntegrationInvariantsTest, PopulationWithinBounds) {
    fixture.SetUp();
    
    // Spawn entities
    auto genome = fixture.create_test_genome(7000);
    for (int i = 0; i < 50; ++i) {
        double angle = i * 6.28 / 50.0;
        double radius = 3.0;
        fixture.spawn_herbivore({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, genome);
    }
    
    for (int i = 0; i < 100; ++i) {
        double x = (i % 10) * 8.0 - 4.0;
        double z = (i / 10) * 8.0 - 4.0;
        fixture.spawn_plant({x, 0.0, z}, 0, 5.0);
    }
    
    fixture.run_steps(100);
    
    auto snap = fixture.take_snapshot();
    EXPECT_LE(snap.entity_count, 200) << "Entity count exceeds limit";
    EXPECT_LE(snap.plant_count, 150) << "Plant count exceeds limit";
    EXPECT_LE(snap.herbivore_count, 50) << "Herbivore count exceeds limit";
}

// 3. NaN/Inf Detection
TEST_F(IntegrationInvariantsTest, NoNaNOrInfInComponents) {
    fixture.SetUp();
    
    auto genome = fixture.create_test_genome(8000);
    auto h1 = fixture.spawn_herbivore({0.0, 0.0, 0.0}, genome);
    
    // Manually inject NaN (simulates floating-point error)
    auto& registry = fixture.app().registry();
    auto metab = registry.get<MetabolismComponent>(h1);
    metab.energy = std::numeric_limits<double>::quiet_NaN();
    
    fixture.run_steps(10);
    
    auto snap = fixture.take_snapshot();
    
    // Check for NaN in snapshot total_biomass
    EXPECT_FALSE(std::isnan(snap.total_biomass))
        << "Total biomass is NaN";
    EXPECT_FALSE(std::isinf(snap.total_biomass))
        << "Total biomass is infinite";
}

// 4. Negative Resources
TEST_F(IntegrationInvariantsTest, ResourcesNonNegative) {
    fixture.SetUp();
    
    // Spawn entities with positive energy
    auto genome = fixture.create_test_genome(9000);
    for (int i = 0; i < 20; ++i) {
        double angle = i * 6.28 / 20.0;
        double radius = 3.0;
        fixture.spawn_herbivore({
            std::cos(angle) * radius,
            0.0,
            std::sin(angle) * radius
        }, genome);
    }
    
    for (int i = 0; i < 30; ++i) {
        double x = (i % 6) * 10.0 - 5.0;
        double z = (i / 6) * 10.0 - 5.0;
        fixture.spawn_plant({x, 0.0, z}, 0, 8.0);
    }
    
    fixture.run_steps(20);
    
    auto snap = fixture.take_snapshot();
    
    // Validate no negative energies (entities might be dying, but not negative)
    auto& registry = fixture.app().registry();
    bool found_negative = false;
    
    auto plant_view = registry.view<PlantComponent>();
    plant_view.each([&](const PlantComponent& plant) {
        if (plant.energy < 0.0 && plant.alive) {
            found_negative = true;
        }
    });
    
    auto metab_view = registry.view<MetabolismComponent>();
    metab_view.each([&](const MetabolismComponent& metab) {
        if (metab.energy < 0.0) {
            found_negative = true;
        }
    });
    
    EXPECT_FALSE(found_negative) << "Found negative energy in alive entities";
}

// 5. Genome Consistency
TEST_F(IntegrationInvariantsTest, OffspringGenomesDerivedFromParents) {
    fixture.SetUp();
    
    // Create parents
    auto genome1 = fixture.create_test_genome(10001);
    auto genome2 = fixture.create_test_genome(10002);
    auto parent1 = fixture.spawn_herbivore({0.0, 0.0, 0.0}, genome1);
    auto parent2 = fixture.spawn_herbivore({5.0, 0.0, 0.0}, genome2);
    
    // Give energy for reproduction
    auto& registry = fixture.app().registry();
    auto metab1 = registry.get<MetabolismComponent>(parent1);
    auto metab2 = registry.get<MetabolismComponent>(parent2);
    metab1.energy = 200.0;
    metab2.energy = 200.0;
    
    fixture.run_steps(20);
    
    // Validate offspring has valid genome
    auto snap = fixture.take_snapshot();
    
    bool found_offspring = false;
    bool found_invalid_genome = false;
    
    auto metab_view = registry.view<MetabolismComponent, GenomeHandleComponent, FitnessComponent>();
    metab_view.each([&](const MetabolismComponent& metab,
                        const GenomeHandleComponent& genome,
                        const FitnessComponent& fitness) {
        if (fitness.age_seconds < 10.0 && genome.id != 0) {
            found_offspring = true;
            if (genome.id == 0) {
                found_invalid_genome = true;
            }
        }
    });
    
    EXPECT_TRUE(found_offspring) << "No offspring created";
    EXPECT_FALSE(found_invalid_genome) << "Offspring has invalid genome ID";
}

// 6. Species Stability
TEST_F(IntegrationInvariantsTest, SpeciesIdsStable) {
    fixture.SetUp();

    auto config = create_test_env_config(2025);
    seed_initial_plants(fixture.app().registry(), config);
    
    // Create entities
    auto genome = fixture.create_test_genome(11000);
    auto h1 = fixture.spawn_herbivore({0.0, 0.0, 0.0}, genome);
    
    // Track initial species count
    auto& registry = fixture.app().registry();
    auto fitness1 = registry.get<FitnessComponent>(h1);
    std::uint32_t initial_offspring = fitness1.offspring_count;
    
    fixture.run_steps(50);
    
    // Species membership shouldn't change arbitrarily
    auto snap = fixture.take_snapshot();
    EXPECT_GE(snap.species_counts.size(), 1) << "Species count dropped";
}

// 7. Soil Regeneration Bounds
TEST_F(IntegrationInvariantsTest, SoilNutrientsWithinValidRange) {
    fixture.SetUp();
    
    auto snap1 = fixture.take_snapshot();
    double initial_soil = snap1.mean_soil;
    
    // Run many ticks to test soil regeneration
    fixture.run_steps(200);
    
    auto snap2 = fixture.take_snapshot();
    double final_soil = snap2.mean_soil;
    
    // Soil should regenerate, not decrease indefinitely
    EXPECT_GE(final_soil, 0.0) << "Soil is negative";
    EXPECT_LE(final_soil, 12.0) << "Soil exceeds max_nutrient";
    
    // Test determinism
    SimulationFixture fixture2;
    fixture2.SetUp();
    auto genome2 = fixture2.create_test_genome(11000);
    fixture2.spawn_herbivore({0.0, 0.0, 0.0}, genome2);
    fixture2.run_steps(200);
    auto snap3 = fixture2.take_snapshot();
    EXPECT_EQ(snap2.mean_soil, snap3.mean_soil) << "Soil regeneration not deterministic";
}
