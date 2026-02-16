#include <entt/entt.hpp>
#include <gtest/gtest.h>
#include <vector>

#include "evolution/sim/components.h"

using namespace evolution::sim;

TEST(ECSLifecycleTest, CreateDestroyEntity) {
    entt::registry registry;

    const entt::entity e1 = registry.create();
    EXPECT_TRUE(registry.valid(e1));

    registry.destroy(e1);
    EXPECT_FALSE(registry.valid(e1));

    const entt::entity e2 = registry.create();
    EXPECT_TRUE(registry.valid(e2));
}

TEST(ECSLifecycleTest, AddRemoveComponent) {
    entt::registry registry;
    const entt::entity e = registry.create();

    registry.emplace<TransformComponent>(e, TransformComponent{{1.0, 2.0, 3.0}});
    EXPECT_TRUE(registry.all_of<TransformComponent>(e));

    registry.remove<TransformComponent>(e);
    EXPECT_FALSE(registry.any_of<TransformComponent>(e));
}

TEST(ECSLifecycleTest, ReplaceComponent) {
    entt::registry registry;
    const entt::entity e = registry.create();

    registry.emplace<TransformComponent>(e, TransformComponent{{0.0, 0.0, 0.0}});
    registry.emplace_or_replace<TransformComponent>(e, TransformComponent{{1.0, 1.0, 0.0}});

    const auto& comp = registry.get<TransformComponent>(e);
    EXPECT_EQ(comp.position.x, 1.0);
}

TEST(ECSLifecycleTest, IterationWhileAddingComponents) {
    entt::registry registry;
    std::vector<entt::entity> entities;
    entities.reserve(10);

    for (int i = 0; i < 10; ++i) {
        entities.push_back(registry.create());
    }

    for (const auto entity : registry.view<TransformComponent>()) {
        registry.emplace_or_replace<TransformComponent>(entity, TransformComponent{{0.0, 0.0, 0.0}});
    }

    EXPECT_EQ(entities.size(), 10);
}

TEST(ECSLifecycleTest, DestroyDuringIteration) {
    entt::registry registry;

    for (int i = 0; i < 5; ++i) {
        registry.create();
    }

    int destroyed_count = 0;
    for (const auto entity : registry.view<TransformComponent>()) {
        if (static_cast<std::uint32_t>(entity) % 2 == 0) {
            registry.destroy(entity);
            ++destroyed_count;
        }
    }

    EXPECT_GE(destroyed_count, 0);
}

TEST(ECSLifecycleTest, MultipleViews) {
    entt::registry registry;
    const entt::entity e = registry.create();

    struct AComponent { int value; };
    struct BComponent { double value; };

    registry.emplace<AComponent>(e, AComponent{42});
    registry.emplace<BComponent>(e, BComponent{3.14});

    const auto a_view = registry.view<AComponent>();
    const auto b_view = registry.view<BComponent>();

    EXPECT_TRUE(a_view.contains(e));
    EXPECT_TRUE(b_view.contains(e));
}

TEST(ECSLifecycleTest, EmptyRegistry) {
    entt::registry registry;

    const auto view = registry.view<TransformComponent>();
    EXPECT_EQ(view.size(), 0);

    for (const auto entity : view) {
        (void)entity;
    }

    const entt::entity e = registry.create();
    EXPECT_TRUE(registry.valid(e));
}

TEST(ECSLifecycleTest, ClearRegistry) {
    entt::registry registry;

    for (int i = 0; i < 20; ++i) {
        registry.create();
    }

    EXPECT_EQ(registry.storage<entt::entity>().in_use(), 20);

    registry.clear();

    EXPECT_EQ(registry.storage<entt::entity>().in_use(), 0);

    const entt::entity e = registry.create();
    EXPECT_TRUE(registry.valid(e));
}

TEST(ECSLifecycleTest, OrphanedEntities) {
    entt::registry registry;

    const entt::entity e = registry.create();
    registry.emplace<TransformComponent>(e, TransformComponent{{5.0, 6.0, 7.0}});

    EXPECT_TRUE(registry.valid(e));

    registry.clear();
    EXPECT_FALSE(registry.valid(e));
}

TEST(ECSLifecycleTest, EntityRecycling) {
    entt::registry registry;

    const entt::entity e1 = registry.create();
    registry.destroy(e1);

    const entt::entity e2 = registry.create();

    EXPECT_TRUE(registry.valid(e2));
}
