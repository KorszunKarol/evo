#include <entt/entt.hpp>
#include <gtest/gtest.h>

#include <vector>

namespace {

struct Transform {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct AComponent {
    int value{0};
};

struct BComponent {
    double value{0.0};
};

}  // namespace

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

    registry.emplace<Transform>(e, Transform{1.0, 2.0, 3.0});
    EXPECT_TRUE(registry.all_of<Transform>(e));

    registry.remove<Transform>(e);
    EXPECT_FALSE(registry.any_of<Transform>(e));
}

TEST(ECSLifecycleTest, ReplaceComponent) {
    entt::registry registry;
    const entt::entity e = registry.create();

    registry.emplace<Transform>(e, Transform{0.0, 0.0, 0.0});
    registry.emplace_or_replace<Transform>(e, Transform{1.0, 1.0, 0.0});

    const auto& comp = registry.get<Transform>(e);
    EXPECT_EQ(comp.x, 1.0);
    EXPECT_EQ(comp.y, 1.0);
    EXPECT_EQ(comp.z, 0.0);
}

TEST(ECSLifecycleTest, MultipleViews) {
    entt::registry registry;
    const entt::entity e = registry.create();

    registry.emplace<AComponent>(e, AComponent{42});
    registry.emplace<BComponent>(e, BComponent{3.14});

    auto a_view = registry.view<AComponent>();
    auto b_view = registry.view<BComponent>();

    EXPECT_TRUE(a_view.contains(e));
    EXPECT_TRUE(b_view.contains(e));

    std::size_t a_count = 0;
    for (auto entity : a_view) {
        (void)entity;
        ++a_count;
    }
    EXPECT_EQ(a_count, 1U);

    std::size_t b_count = 0;
    for (auto entity : b_view) {
        (void)entity;
        ++b_count;
    }
    EXPECT_EQ(b_count, 1U);
}

TEST(ECSLifecycleTest, EmptyRegistryView) {
    entt::registry registry;

    auto view = registry.view<Transform>();
    for (auto entity : view) {
        (void)entity;
        FAIL() << "View should be empty";
    }
}

TEST(ECSLifecycleTest, ClearRegistry) {
    entt::registry registry;

    std::vector<entt::entity> entities;
    entities.reserve(20);
    for (int i = 0; i < 20; ++i) {
        entities.push_back(registry.create());
    }

    // Attach at least one component so storage usage is nonzero.
    for (auto e : entities) {
        registry.emplace<Transform>(e, Transform{5.0, 6.0, 7.0});
    }

    EXPECT_EQ(registry.storage<entt::entity>().in_use(), 20U);

    registry.clear();

    EXPECT_EQ(registry.storage<entt::entity>().in_use(), 0U);
}

TEST(ECSLifecycleTest, EntityRecycling) {
    entt::registry registry;

    const entt::entity e1 = registry.create();
    registry.destroy(e1);
    EXPECT_FALSE(registry.valid(e1));

    const entt::entity e2 = registry.create();
    EXPECT_TRUE(registry.valid(e2));

    // EnTT encodes the version in the entity value; reused ids should differ.
    EXPECT_NE(e1, e2);
}
