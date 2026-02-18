#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/creature_spatial_index.h"

using namespace evolution::sim;

TEST(CreatureSpatialIndex, QueryReturnsExpectedSubset) {
    entt::registry registry;
    CreatureSpatialIndex index(2.0);

    const entt::entity a = registry.create();
    registry.emplace<TransformComponent>(a, TransformComponent{Vec3{0.0, 0.0, 0.0}});
    registry.emplace<MetabolismComponent>(a);

    const entt::entity b = registry.create();
    registry.emplace<TransformComponent>(b, TransformComponent{Vec3{1.0, 0.0, 1.0}});
    registry.emplace<MetabolismComponent>(b);

    const entt::entity c = registry.create();
    registry.emplace<TransformComponent>(c, TransformComponent{Vec3{10.0, 0.0, 10.0}});
    registry.emplace<MetabolismComponent>(c);

    index.rebuild(registry);

    std::vector<entt::entity> found;
    index.for_each_in_radius(registry, Vec3{0.0, 0.0, 0.0}, 2.0, [&](entt::entity e, double) {
        found.push_back(e);
    });

    EXPECT_EQ(found.size(), 2u);
    EXPECT_NE(std::find(found.begin(), found.end(), a), found.end());
    EXPECT_NE(std::find(found.begin(), found.end(), b), found.end());
    EXPECT_EQ(std::find(found.begin(), found.end(), c), found.end());
}

TEST(CreatureSpatialIndex, EmptyIndexReturnsNoResults) {
    entt::registry registry;
    CreatureSpatialIndex index(2.0);

    std::size_t hit_count = 0;
    index.for_each_in_radius(registry, Vec3{0.0, 0.0, 0.0}, 5.0, [&](entt::entity, double) { ++hit_count; });
    EXPECT_EQ(hit_count, 0u);
}

TEST(CreatureSpatialIndex, BoundaryDistanceIncluded) {
    entt::registry registry;
    CreatureSpatialIndex index(2.0);

    const entt::entity e = registry.create();
    registry.emplace<TransformComponent>(e, TransformComponent{Vec3{3.0, 0.0, 4.0}});
    registry.emplace<MetabolismComponent>(e);

    index.rebuild(registry);

    std::size_t hit_count = 0;
    index.for_each_in_radius(registry, Vec3{0.0, 0.0, 0.0}, 5.0, [&](entt::entity, double) { ++hit_count; });
    EXPECT_EQ(hit_count, 1u);
}

TEST(CreatureSpatialIndex, RebuildDeterministicForIdenticalPositions) {
    entt::registry registry;
    CreatureSpatialIndex index(2.5);

    for (int i = 0; i < 30; ++i) {
        const entt::entity e = registry.create();
        registry.emplace<TransformComponent>(e,
                                             TransformComponent{Vec3{static_cast<double>(i % 7), 0.0, static_cast<double>(i / 7)}});
        registry.emplace<MetabolismComponent>(e);
    }

    auto run_query = [&]() {
        std::vector<entt::entity> result;
        index.rebuild(registry);
        index.for_each_in_radius(registry, Vec3{3.0, 0.0, 2.0}, 3.5, [&](entt::entity e, double) {
            result.push_back(e);
        });
        std::sort(result.begin(), result.end());
        return result;
    };

    const auto first = run_query();
    const auto second = run_query();
    EXPECT_EQ(first, second);
}
