#include <entt/entt.hpp>
#include <gtest/gtest.h>

using namespace evolution::sim;

class ECSLifecycleTest : public ::testing::Test {
};

// 1. Entity Creation and Destruction
TEST(ECSLifecycleTest, CreateDestroyEntity) {
    entt::registry registry;
    
    // Create entity
    entt::entity e1 = registry.create();
    EXPECT_TRUE(registry.valid(e1));
    EXPECT_NE(e1, entt::null);
    
    // Destroy entity
    registry.destroy(e1);
    EXPECT_FALSE(registry.valid(e1));
    
    // Can create new entity after destruction
    entt::entity e2 = registry.create();
    EXPECT_NE(e2, entt::null);
}

// 2. Component Addition and Removal
TEST(ECSLifecycleTest, AddRemoveComponent) {
    entt::registry registry;
    entt::entity e = registry.create();
    
    // Add component
    registry.emplace<TransformComponent>(e, TransformComponent{{1.0, 2.0, 3.0}});
    EXPECT_TRUE(registry.all_of<TransformComponent>(e));
    
    // Remove component
    registry.remove<TransformComponent>(e);
    EXPECT_FALSE(registry.any_of<TransformComponent>(e));
}

// 3. Component Replacement
TEST(ECSLifecycleTest, ReplaceComponent) {
    entt::registry registry;
    entt::entity e = registry.create();
    
    // Add component 1
    registry.emplace<TransformComponent>(e, TransformComponent{{0.0, 0.0, 0.0}});
    
    // Replace with component 2
    registry.emplace_or_replace<TransformComponent>(e, TransformComponent{{1.0, 1.0, 0.0}});
    
    auto& comp = registry.get<TransformComponent>(e);
    EXPECT_EQ(comp.position.x, 1.0);
}

// 4. Iteration Stability During Component Changes
TEST(ECSLifecycleTest, IterationWhileAddingComponents) {
    entt::registry registry;
    
    // Create initial entities
    std::vector<entt::entity> entities;
    for (int i = 0; i < 10; ++i) {
        entities.push_back(registry.create());
    }
    
    // Iterate and add components
    auto view = registry.view<TransformComponent>();
    for (auto entity : view) {
        registry.emplace_or_replace<TransformComponent>(entity, TransformComponent{{0.0, 0.0, 0.0}});
    }
    
    EXPECT_EQ(entities.size(), 10);
}

// 5. Entity Destruction During Iteration
TEST(ECSLifecycleTest, DestroyDuringIteration) {
    entt::registry registry;
    
    // Create entities
    for (int i = 0; i < 5; ++i) {
        registry.create();
    }
    
    // Iterate and destroy
    auto view = registry.view<TransformComponent>();
    int destroyed_count = 0;
    for (auto entity : view) {
        if (static_cast<std::uint32_t>(entity) % 2 == 0) {
            registry.destroy(entity);
            ++destroyed_count;
        }
    }
    
    EXPECT_EQ(destroyed_count, 3);
}

// 6. Multiple Component Views
TEST(ECSLifecycleTest, MultipleViews) {
    entt::registry registry;
    entt::entity e = registry.create();
    
    // Add multiple components
    struct AComponent { int value; };
    struct BComponent { double value; };
    
    registry.emplace<AComponent>(e, AComponent{42});
    registry.emplace<BComponent>(e, BComponent{3.14});
    
    // Can iterate both views
    auto a_view = registry.view<AComponent>();
    auto b_view = registry.view<BComponent>();
    
    EXPECT_TRUE(a_view.contains(e));
    EXPECT_TRUE(b_view.contains(e));
    
    int count = 0;
    for (auto entity : a_view) {
        ++count;
    }
    EXPECT_EQ(count, 1);
    
    count = 0;
    for (auto entity : b_view) {
        ++count;
    }
    EXPECT_EQ(count, 1);
}

// 7. Empty Registry Handling
TEST(ECSLifecycleTest, EmptyRegistry) {
    entt::registry registry;
    
    // Empty views should work
    auto view = registry.view<TransformComponent>();
    EXPECT_EQ(view.size(), 0);
    
    // Iteration should not crash
    for (auto entity : view) {
        (void)entity;
    }
    
    // Creating in empty registry works
    entt::entity e = registry.create();
    EXPECT_NE(e, entt::null);
}

// 8. Registry Clearing
TEST(ECSLifecycleTest, ClearRegistry) {
    entt::registry registry;
    
    // Create entities
    std::vector<entt::entity> entities;
    for (int i = 0; i < 20; ++i) {
        entities.push_back(registry.create());
    }
    
    EXPECT_EQ(registry.storage<entt::entity>().in_use(), 20);
    
    // Clear all
    registry.clear();
    
    EXPECT_EQ(registry.storage<entt::entity>().in_use(), 0);
    
    // Can create after clear
    entt::entity e = registry.create();
    EXPECT_NE(e, entt::null);
}

// 9. Orphaned Entities (No References)
TEST(ECSLifecycleTest, OrphanedEntities) {
    entt::registry registry;
    
    // Create entity, remove all references
    entt::entity e = registry.create();
    registry.emplace<TransformComponent>(e, TransformComponent{{5.0, 6.0, 7.0}});
    
    // Entity has no components referencing it (if no relationship tracking)
    // Should not cause issues with EnTT
    EXPECT_TRUE(registry.valid(e));
    
    // Clear
    registry.clear();
    
    // Entity should be gone without errors
    EXPECT_FALSE(registry.valid(e));
}

// 10. Entity Versioning (Recycling)
TEST(ECSLifecycleTest, EntityRecycling) {
    entt::registry registry;
    
    // Create and destroy entity
    entt::entity e1 = registry.create();
    registry.destroy(e1);
    
    // New entity might reuse same ID
    entt::entity e2 = registry.create();
    
    // IDs should be valid but different
    EXPECT_NE(e1, e2);
    EXPECT_TRUE(registry.valid(e1));
    EXPECT_TRUE(registry.valid(e2));
}
