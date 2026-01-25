#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/creature_spatial_index.h"
#include "evolution/sim/creature_spatial_index_system.h"
#include "evolution/sim/simulation_context.h"

namespace evolution::sim {

TEST(CreatureSpatialIndexSystem, RebuildsAndQueriesDeterministically) {
    entt::registry registry;

    auto spawn_creature = [&](const Vec3& position) {
        const entt::entity entity = registry.create();
        registry.emplace<TransformComponent>(entity, TransformComponent{.position = position});
        registry.emplace<MetabolismComponent>(entity, MetabolismComponent{.energy = 10.0});
        registry.emplace<DietComponent>(entity, DietComponent{.type = DietType::Herbivore});
        return entity;
    };

    const entt::entity a = spawn_creature(Vec3{0.0, 0.0, 0.0});
    const entt::entity b = spawn_creature(Vec3{1.0, 0.0, 0.0});
    [[maybe_unused]] const entt::entity c = spawn_creature(Vec3{5.0, 0.0, 0.0});

    CreatureSpatialIndexSystem system(1.0);
    SimulationContext context(registry, 0.016, 0.0);
    system.tick(context);

    const auto& index = registry.ctx().get<CreatureSpatialIndex>();
    std::vector<entt::entity> found;
    index.for_each_in_radius(registry, Vec3{0.0, 0.0, 0.0}, 2.0,
                             [&](entt::entity entity) {
                                 found.push_back(entity);
                                 return true;
                             });

    std::sort(found.begin(), found.end());
    std::vector<entt::entity> expected{a, b};
    std::sort(expected.begin(), expected.end());

    EXPECT_EQ(found, expected);
    EXPECT_EQ(found.size(), 2U);
}

}  // namespace evolution::sim
