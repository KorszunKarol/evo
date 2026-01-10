#include "test_fixtures.h"

#include <cmath>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/feeding_system.h"
#include "evolution/sim/environment/plant_systems.h"
#include "evolution/sim/simulation_context.h"

using namespace evolution::sim::test;
using namespace evolution::sim;

TEST(FeedingSystem, HerbivoreConsumesPlant) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    // 1. Setup Environment Systems
    if (!registry.ctx().contains<PlantSpatialIndex>()) {
        registry.ctx().emplace<PlantSpatialIndex>(10.0);
    }
    PlantSpatialSystem spatial_system;
    FeedingSystem feeding_system;

    // 2. Spawn Plant
    const Vec3 plant_pos{10.0, 0.0, 10.0};
    entt::entity plant = fixture.spawn_plant(plant_pos, 0, 20.0); // Energy = 20
    auto& plant_comp = registry.get<PlantComponent>(plant);
    plant_comp.radius = 1.0;

    // 3. Spawn Herbivore
    const Vec3 herbivore_pos{11.0, 0.0, 10.0}; // Distance = 1.0
    const auto genome_id = fixture.create_test_genome();
    entt::entity herbivore = fixture.spawn_herbivore(herbivore_pos, genome_id);

    // Force Diet to Herbivore (default in spawn might vary based on ID)
    auto& diet = registry.emplace_or_replace<DietComponent>(herbivore);
    diet.type = DietType::Herbivore;
    registry.emplace_or_replace<HerbivoreTag>(herbivore);

    // Setup Feeding Intent
    auto& intent = registry.get_or_emplace<FeedingIntent>(herbivore);
    intent.request_eat = true;
    intent.reach = 2.0;
    intent.rate = 10.0;

    auto& metabolism = registry.get<MetabolismComponent>(herbivore);
    metabolism.energy = 50.0;
    metabolism.max_energy = 100.0;

    // 4. Tick
    SimulationContext context(registry, 0.1, 0.0); // dt = 0.1s
    
    // Must update spatial index first
    spatial_system.tick(context);
    feeding_system.tick(context);

    // 5. Verify
    // Rate 10.0 * 0.1s = 1.0 energy transferred
    EXPECT_NEAR(metabolism.energy, 51.0, 1e-6);
    EXPECT_NEAR(plant_comp.energy, 19.0, 1e-6);
}

TEST(FeedingSystem, HerbivoreIgnoresDistantPlant) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    if (!registry.ctx().contains<PlantSpatialIndex>()) {
        registry.ctx().emplace<PlantSpatialIndex>(10.0);
    }
    PlantSpatialSystem spatial_system;
    FeedingSystem feeding_system;

    const Vec3 plant_pos{10.0, 0.0, 10.0};
    entt::entity plant = fixture.spawn_plant(plant_pos, 0, 20.0);
    auto& plant_comp = registry.get<PlantComponent>(plant);
    plant_comp.radius = 1.0;

    const Vec3 herbivore_pos{20.0, 0.0, 10.0}; // Distance = 10.0
    const auto genome_id = fixture.create_test_genome();
    entt::entity herbivore = fixture.spawn_herbivore(herbivore_pos, genome_id);

    auto& diet = registry.emplace_or_replace<DietComponent>(herbivore);
    diet.type = DietType::Herbivore;

    auto& intent = registry.get_or_emplace<FeedingIntent>(herbivore);
    intent.request_eat = true;
    intent.reach = 2.0;
    intent.rate = 10.0;

    auto& metabolism = registry.get<MetabolismComponent>(herbivore);
    metabolism.energy = 50.0;

    SimulationContext context(registry, 0.1, 0.0);
    spatial_system.tick(context);
    feeding_system.tick(context);

    EXPECT_DOUBLE_EQ(metabolism.energy, 50.0);
    EXPECT_DOUBLE_EQ(plant_comp.energy, 20.0);
}

TEST(FeedingSystem, CarnivoreConsumesPrey) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();
    FeedingSystem feeding_system;

    // 1. Spawn Carnivore
    const Vec3 carnivore_pos{0.0, 0.0, 0.0};
    const auto carnivore_genome = fixture.create_test_genome(123);
    entt::entity carnivore = fixture.spawn_herbivore(carnivore_pos, carnivore_genome); // Spawn helper generic

    auto& c_diet = registry.emplace_or_replace<DietComponent>(carnivore);
    c_diet.type = DietType::Carnivore;
    registry.remove<HerbivoreTag>(carnivore); // Just in case

    auto& c_intent = registry.get_or_emplace<FeedingIntent>(carnivore);
    c_intent.request_eat = true;
    c_intent.reach = 2.0;
    c_intent.rate = 20.0; // 20 per sec
    
    // Set attack intent (required for carnivore feeding)
    auto& c_actuation = registry.get_or_emplace<ActuationComponent>(carnivore);
    c_actuation.attack = true;

    auto& c_metab = registry.get<MetabolismComponent>(carnivore);
    c_metab.energy = 50.0;
    c_metab.max_energy = 100.0;

    // 2. Spawn Prey
    const Vec3 prey_pos{1.0, 0.0, 0.0}; // Distance 1.0
    const auto prey_genome = fixture.create_test_genome(456);
    entt::entity prey = fixture.spawn_herbivore(prey_pos, prey_genome);
    
    // Prey is generic (herbivore by default or random, doesn't matter for being eaten)
    auto& p_metab = registry.get<MetabolismComponent>(prey);
    p_metab.energy = 50.0;

    // 3. Tick
    SimulationContext context(registry, 0.1, 0.0); // dt=0.1
    feeding_system.tick(context);

    // 4. Verify
    // Rate 20.0 * 0.1 = 2.0 transferred
    EXPECT_NEAR(c_metab.energy, 52.0, 1e-6);
    EXPECT_NEAR(p_metab.energy, 48.0, 1e-6);
}

TEST(FeedingSystem, CarnivoreIgnoresDistantPrey) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();
    FeedingSystem feeding_system;

    const Vec3 c_pos{0.0, 0.0, 0.0};
    entt::entity carnivore = fixture.spawn_herbivore(c_pos, fixture.create_test_genome(1));
    registry.emplace_or_replace<DietComponent>(carnivore, DietComponent{DietType::Carnivore});
    auto& c_intent = registry.get_or_emplace<FeedingIntent>(carnivore);
    c_intent.request_eat = true;
    c_intent.reach = 2.0;
    
    // Set attack intent (required for carnivore feeding)
    auto& c_actuation = registry.get_or_emplace<ActuationComponent>(carnivore);
    c_actuation.attack = true;

    auto& c_metab = registry.get<MetabolismComponent>(carnivore);
    c_metab.energy = 50.0;

    const Vec3 p_pos{5.0, 0.0, 0.0}; // Distance 5.0 > Reach 2.0
    entt::entity prey = fixture.spawn_herbivore(p_pos, fixture.create_test_genome(2));
    auto& p_metab = registry.get<MetabolismComponent>(prey);
    p_metab.energy = 50.0;

    SimulationContext context(registry, 0.1, 0.0);
    feeding_system.tick(context);

    EXPECT_DOUBLE_EQ(c_metab.energy, 50.0);
    EXPECT_DOUBLE_EQ(p_metab.energy, 50.0);
}

TEST(FeedingSystem, CarnivoreRequiresAttackIntent) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();
    FeedingSystem feeding_system;

    // Spawn Carnivore without attack intent
    const Vec3 c_pos{0.0, 0.0, 0.0};
    entt::entity carnivore = fixture.spawn_herbivore(c_pos, fixture.create_test_genome(1));
    registry.emplace_or_replace<DietComponent>(carnivore, DietComponent{DietType::Carnivore});
    auto& c_intent = registry.get_or_emplace<FeedingIntent>(carnivore);
    c_intent.request_eat = true;
    c_intent.reach = 2.0;
    c_intent.rate = 20.0;
    
    // Attack intent is FALSE
    auto& c_actuation = registry.get_or_emplace<ActuationComponent>(carnivore);
    c_actuation.attack = false;

    auto& c_metab = registry.get<MetabolismComponent>(carnivore);
    c_metab.energy = 50.0;

    // Spawn Prey within reach
    const Vec3 p_pos{1.0, 0.0, 0.0};
    entt::entity prey = fixture.spawn_herbivore(p_pos, fixture.create_test_genome(2));
    auto& p_metab = registry.get<MetabolismComponent>(prey);
    p_metab.energy = 50.0;

    SimulationContext context(registry, 0.1, 0.0);
    feeding_system.tick(context);

    // No attack without intent - both energies unchanged
    EXPECT_DOUBLE_EQ(c_metab.energy, 50.0);
    EXPECT_DOUBLE_EQ(p_metab.energy, 50.0);
}

TEST(FeedingSystem, CarnivoreAttackCooldown) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();
    FeedingSystem feeding_system;

    // Spawn Carnivore with combat component
    const Vec3 c_pos{0.0, 0.0, 0.0};
    entt::entity carnivore = fixture.spawn_herbivore(c_pos, fixture.create_test_genome(1));
    registry.emplace_or_replace<DietComponent>(carnivore, DietComponent{DietType::Carnivore});
    auto& c_intent = registry.get_or_emplace<FeedingIntent>(carnivore);
    c_intent.request_eat = true;
    c_intent.reach = 2.0;
    c_intent.rate = 20.0;
    
    auto& c_actuation = registry.get_or_emplace<ActuationComponent>(carnivore);
    c_actuation.attack = true;
    
    // Combat component with active cooldown
    auto& combat = registry.emplace_or_replace<CombatComponent>(carnivore);
    combat.attack_cooldown = 1.0;
    combat.attack_timer = 0.5;  // In cooldown

    auto& c_metab = registry.get<MetabolismComponent>(carnivore);
    c_metab.energy = 50.0;

    // Spawn Prey within reach
    const Vec3 p_pos{1.0, 0.0, 0.0};
    entt::entity prey = fixture.spawn_herbivore(p_pos, fixture.create_test_genome(2));
    auto& p_metab = registry.get<MetabolismComponent>(prey);
    p_metab.energy = 50.0;

    SimulationContext context(registry, 0.1, 0.0);
    feeding_system.tick(context);

    // No attack during cooldown
    EXPECT_DOUBLE_EQ(c_metab.energy, 50.0);
    EXPECT_DOUBLE_EQ(p_metab.energy, 50.0);
    
    // Cooldown should have decremented
    EXPECT_NEAR(combat.attack_timer, 0.4, 1e-6);
}
