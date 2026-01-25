#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/creature_spatial_index_system.h"
#include "evolution/sim/social_behavior_system.h"
#include "evolution/sim/simulation_context.h"

namespace evolution::sim {

namespace {

entt::entity SpawnCreature(entt::registry& registry,
                           const Vec3& position,
                           const Vec3& velocity,
                           DietType diet) {
    const entt::entity entity = registry.create();
    registry.emplace<TransformComponent>(entity, TransformComponent{.position = position});
    registry.emplace<KinematicsComponent>(entity, KinematicsComponent{.linear_velocity = velocity});
    registry.emplace<MetabolismComponent>(entity, MetabolismComponent{.energy = 10.0});
    registry.emplace<DietComponent>(entity, DietComponent{.type = diet});
    if (diet == DietType::Herbivore) {
        registry.emplace<HerbivoreTag>(entity);
    } else {
        registry.emplace<CarnivoreTag>(entity);
    }
    return entity;
}

}  // namespace

TEST(SocialBehaviorSystem, ComputesFlockingSignals) {
    entt::registry registry;

    const entt::entity self =
        SpawnCreature(registry, Vec3{0.0, 0.0, 0.0}, Vec3{0.0, 0.0, 0.0}, DietType::Herbivore);
    SpawnCreature(registry, Vec3{2.0, 0.0, 0.0}, Vec3{1.0, 0.0, 0.0}, DietType::Herbivore);
    SpawnCreature(registry, Vec3{0.0, 0.0, 2.0}, Vec3{0.0, 0.0, 1.0}, DietType::Herbivore);

    CreatureSpatialIndexSystem index_system(1.0);
    SocialBehaviorSystem social_system;
    SimulationContext context(registry, 0.016, 0.0);

    index_system.tick(context);
    social_system.tick(context);

    const auto& signals = registry.get<SocialSignalsComponent>(self);
    EXPECT_NEAR(signals.cohesion_dir.x, 0.7071, 1e-3);
    EXPECT_NEAR(signals.cohesion_dir.z, 0.7071, 1e-3);
    EXPECT_NEAR(signals.alignment_dir.x, 0.7071, 1e-3);
    EXPECT_NEAR(signals.alignment_dir.z, 0.7071, 1e-3);
    EXPECT_NEAR(signals.separation_dir.x, -0.7071, 1e-3);
    EXPECT_NEAR(signals.separation_dir.z, -0.7071, 1e-3);
}

TEST(SocialBehaviorSystem, ComputesTerritorialSignals) {
    entt::registry registry;

    const entt::entity self =
        SpawnCreature(registry, Vec3{6.0, 0.0, 0.0}, Vec3{0.0, 0.0, 0.0}, DietType::Herbivore);
    SpawnCreature(registry, Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 0.0, 0.0}, DietType::Carnivore);

    auto& territory = registry.emplace<TerritoryComponent>(self);
    territory.center = Vec3{0.0, 0.0, 0.0};
    territory.radius = 12.0;
    territory.initialized = true;

    CreatureSpatialIndexSystem index_system(1.0);
    SocialBehaviorSystem social_system;
    SimulationContext context(registry, 0.016, 0.0);

    index_system.tick(context);
    social_system.tick(context);

    const auto& signals = registry.get<SocialSignalsComponent>(self);
    EXPECT_NEAR(signals.territory_dist_norm, 0.5, 1e-6);
    EXPECT_NEAR(signals.intruder_density, 0.25, 1e-6);
}

TEST(SocialBehaviorSystem, ComputesPackHuntingSignals) {
    entt::registry registry;

    const entt::entity self =
        SpawnCreature(registry, Vec3{0.0, 0.0, 0.0}, Vec3{0.0, 0.0, 0.0}, DietType::Carnivore);
    SpawnCreature(registry, Vec3{4.0, 0.0, 0.0}, Vec3{0.0, 0.0, 0.0}, DietType::Herbivore);
    SpawnCreature(registry, Vec3{4.0, 0.0, 1.0}, Vec3{0.0, 0.0, 0.0}, DietType::Carnivore);

    CreatureSpatialIndexSystem index_system(1.0);
    SocialBehaviorSystem social_system;
    SimulationContext context(registry, 0.016, 0.0);

    index_system.tick(context);
    social_system.tick(context);

    const auto& signals = registry.get<SocialSignalsComponent>(self);
    EXPECT_NEAR(signals.prey_dir.x, 1.0, 1e-6);
    EXPECT_NEAR(signals.prey_dir.z, 0.0, 1e-6);
    EXPECT_NEAR(signals.pack_density_near_prey, 0.25, 1e-6);
}

}  // namespace evolution::sim
