#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/creature_spatial_index.h"
#include "evolution/sim/environment/feeding_system.h"
#include "evolution/sim/simulation_context.h"

using namespace evolution::sim;

namespace {

entt::entity spawn_predator(entt::registry& registry, const Vec3& pos) {
    const entt::entity e = registry.create();
    registry.emplace<TransformComponent>(e, TransformComponent{pos});
    registry.emplace<MetabolismComponent>(e, MetabolismComponent{.energy = 40.0, .max_energy = 100.0, .basal_rate = 1.0});
    registry.emplace<FeedingIntent>(e, FeedingIntent{.request_eat = true, .reach = 1.6, .rate = 20.0});
    registry.emplace<DietComponent>(e, DietComponent{DietType::Carnivore});
    registry.emplace<CarnivoreTag>(e);
    registry.emplace<ActuationComponent>(e, ActuationComponent{.attack = true});
    registry.emplace<CombatComponent>(e,
                                      CombatComponent{.attack_cooldown = 0.5,
                                                      .attack_timer = 0.0,
                                                      .target = entt::null,
                                                      .damage_dealt = 0.0,
                                                      .attack_power = 40.0,
                                                      .attack_reach = 1.6,
                                                      .conversion_efficiency = 0.75});
    registry.emplace<PursuitComponent>(e, PursuitComponent{.target_entity = entt::null,
                                                           .pursuit_time = 0.0,
                                                           .max_pursuit_time = 10.0,
                                                           .engage_distance = 8.0});
    return e;
}

entt::entity spawn_prey(entt::registry& registry, const Vec3& pos, double health = 10.0) {
    const entt::entity e = registry.create();
    registry.emplace<TransformComponent>(e, TransformComponent{pos});
    registry.emplace<MetabolismComponent>(e, MetabolismComponent{.energy = 30.0, .max_energy = 60.0, .basal_rate = 1.0});
    registry.emplace<HealthComponent>(e, HealthComponent{.health = health, .max_health = health, .regen_rate = 0.0});
    registry.emplace<DietComponent>(e, DietComponent{DietType::Herbivore});
    registry.emplace<HerbivoreTag>(e);
    return e;
}

}  // namespace

TEST(AdvancedPredation, DealsDamageAndAppliesCooldown) {
    entt::registry registry;
    registry.ctx().emplace<CreatureSpatialIndex>(2.0);

    const entt::entity predator = spawn_predator(registry, Vec3{0.0, 0.0, 0.0});
    const entt::entity prey = spawn_prey(registry, Vec3{1.0, 0.0, 0.0}, 20.0);

    registry.ctx().get<CreatureSpatialIndex>().rebuild(registry);

    FeedingSystem system;
    SimulationContext context(registry, 0.1, 0.0);
    system.tick(context);

    const auto& prey_health = registry.get<HealthComponent>(prey);
    const auto& combat = registry.get<CombatComponent>(predator);

    EXPECT_LT(prey_health.health, 20.0);
    EXPECT_GT(combat.attack_timer, 0.0);
    EXPECT_EQ(combat.target, prey);
}

TEST(AdvancedPredation, KillTransfersEnergyAndDestroysPrey) {
    entt::registry registry;
    registry.ctx().emplace<CreatureSpatialIndex>(2.0);

    const entt::entity predator = spawn_predator(registry, Vec3{0.0, 0.0, 0.0});
    const entt::entity prey = spawn_prey(registry, Vec3{1.0, 0.0, 0.0}, 1.0);

    const double predator_energy_before = registry.get<MetabolismComponent>(predator).energy;

    registry.ctx().get<CreatureSpatialIndex>().rebuild(registry);

    FeedingSystem system;
    SimulationContext context(registry, 0.2, 0.0);
    system.tick(context);

    EXPECT_FALSE(registry.valid(prey));
    EXPECT_GT(registry.get<MetabolismComponent>(predator).energy, predator_energy_before);
}

TEST(AdvancedPredation, CarnivoreDoesNotAttackCarnivore) {
    entt::registry registry;
    registry.ctx().emplace<CreatureSpatialIndex>(2.0);

    const entt::entity predator = spawn_predator(registry, Vec3{0.0, 0.0, 0.0});
    const entt::entity other_carnivore = spawn_predator(registry, Vec3{1.0, 0.0, 0.0});

    registry.ctx().get<CreatureSpatialIndex>().rebuild(registry);

    FeedingSystem system;
    SimulationContext context(registry, 0.1, 0.0);
    system.tick(context);

    const auto& combat = registry.get<CombatComponent>(predator);
    EXPECT_TRUE(combat.target == entt::null);

    const auto& other_health = registry.try_get<HealthComponent>(other_carnivore);
    EXPECT_EQ(other_health, nullptr);
}
