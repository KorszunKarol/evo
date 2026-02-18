#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/creature_spatial_index.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/perception/vision_system.h"
#include "evolution/sim/simulation_context.h"

using namespace evolution::sim;

namespace {

entt::entity make_creature(entt::registry& registry, const Vec3& pos, bool carnivore) {
    const entt::entity e = registry.create();
    registry.emplace<TransformComponent>(e, TransformComponent{pos});
    registry.emplace<KinematicsComponent>(e, KinematicsComponent{});
    registry.emplace<MetabolismComponent>(e, MetabolismComponent{});
    registry.emplace<VisionComponent>(e, VisionComponent{.fov_degrees = 120.0, .num_rays = 8, .max_range = 15.0, .eye_height_offset = 0.3});
    registry.emplace<VisionResult>(e);
    registry.emplace<HeadingComponent>(e, HeadingComponent{.forward = Vec3{1.0, 0.0, 0.0}});
    registry.emplace<DietComponent>(e, DietComponent{carnivore ? DietType::Carnivore : DietType::Herbivore});
    if (carnivore) {
        registry.emplace<CarnivoreTag>(e);
    } else {
        registry.emplace<HerbivoreTag>(e);
    }
    return e;
}

}  // namespace

TEST(VisionSystem, DetectsPlantInFrontRay) {
    entt::registry registry;
    registry.ctx().emplace<PlantSpatialIndex>(2.0);
    registry.ctx().emplace<CreatureSpatialIndex>(2.0);

    const entt::entity observer = make_creature(registry, Vec3{0.0, 0.0, 0.0}, false);

    const entt::entity plant = registry.create();
    registry.emplace<TransformComponent>(plant, TransformComponent{Vec3{4.0, 0.0, 0.0}});
    registry.emplace<PlantComponent>(plant, PlantComponent{.energy = 10.0, .max_energy = 20.0, .radius = 0.5, .alive = true});

    registry.ctx().get<PlantSpatialIndex>().rebuild(registry);
    registry.ctx().get<CreatureSpatialIndex>().rebuild(registry);

    VisionSystem system;
    SimulationContext context(registry, 1.0 / 60.0, 0.0);
    system.tick(context);

    const auto& result = registry.get<VisionResult>(observer);
    ASSERT_EQ(result.rays.size(), 8u);

    bool saw_plant = false;
    for (const auto& ray : result.rays) {
        if (ray.hit_type == VisionHitType::Plant) {
            saw_plant = true;
            EXPECT_LT(ray.distance_normalized, 1.0);
        }
    }
    EXPECT_TRUE(saw_plant);
}

TEST(VisionSystem, FovClippingExcludesTargetBehindObserver) {
    entt::registry registry;
    registry.ctx().emplace<PlantSpatialIndex>(2.0);
    registry.ctx().emplace<CreatureSpatialIndex>(2.0);

    const entt::entity observer = make_creature(registry, Vec3{0.0, 0.0, 0.0}, false);

    const entt::entity plant = registry.create();
    registry.emplace<TransformComponent>(plant, TransformComponent{Vec3{-3.0, 0.0, 0.0}});
    registry.emplace<PlantComponent>(plant, PlantComponent{.energy = 10.0, .max_energy = 20.0, .radius = 0.5, .alive = true});

    registry.ctx().get<PlantSpatialIndex>().rebuild(registry);
    registry.ctx().get<CreatureSpatialIndex>().rebuild(registry);

    VisionSystem system;
    SimulationContext context(registry, 1.0 / 60.0, 0.0);
    system.tick(context);

    const auto& result = registry.get<VisionResult>(observer);
    for (const auto& ray : result.rays) {
        EXPECT_NE(ray.hit_type, VisionHitType::Plant);
    }
}

TEST(VisionSystem, ClassifiesCreatureTypes) {
    entt::registry registry;
    registry.ctx().emplace<PlantSpatialIndex>(2.0);
    registry.ctx().emplace<CreatureSpatialIndex>(2.0);

    const entt::entity observer = make_creature(registry, Vec3{0.0, 0.0, 0.0}, false);
    const entt::entity herb = make_creature(registry, Vec3{5.0, 0.0, 1.0}, false);
    const entt::entity carn = make_creature(registry, Vec3{5.0, 0.0, -1.0}, true);
    (void)herb;
    (void)carn;

    registry.ctx().get<PlantSpatialIndex>().rebuild(registry);
    registry.ctx().get<CreatureSpatialIndex>().rebuild(registry);

    VisionSystem system;
    SimulationContext context(registry, 1.0 / 60.0, 0.0);
    system.tick(context);

    const auto& result = registry.get<VisionResult>(observer);
    bool saw_herb = false;
    bool saw_carn = false;
    for (const auto& ray : result.rays) {
        saw_herb = saw_herb || ray.hit_type == VisionHitType::Herbivore;
        saw_carn = saw_carn || ray.hit_type == VisionHitType::Carnivore;
    }

    EXPECT_TRUE(saw_herb);
    EXPECT_TRUE(saw_carn);
    EXPECT_EQ(result.buffer.size(), 8u * 3u);
}
