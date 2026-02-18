#include <memory>

#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/physics/simple_backend.h"
#include "evolution/sim/physics_system.h"
#include "evolution/sim/simulation_context.h"

using namespace evolution::sim;

namespace {

entt::entity spawn_body(entt::registry& registry, const Vec3& pos) {
    const entt::entity e = registry.create();
    registry.emplace<TransformComponent>(e, TransformComponent{pos});
    registry.emplace<KinematicsComponent>(e, KinematicsComponent{.inverse_mass = 1.0});
    ColliderComponent collider{};
    collider.type = ShapeType::Sphere;
    collider.sphere.radius = 0.6;
    registry.emplace<ColliderComponent>(e, collider);
    registry.emplace<RigidbodyComponent>(e);
    registry.emplace<ContactSenseComponent>(e);
    return e;
}

}  // namespace

TEST(ContactSense, SummarizesCollisionContacts) {
    entt::registry registry;

    const entt::entity a = spawn_body(registry, Vec3{0.0, 3.0, 0.0});
    const entt::entity b = spawn_body(registry, Vec3{0.8, 3.0, 0.0});

    auto backend = std::make_unique<SimplePhysicsBackend>(SimplePhysicsConfig{});
    PhysicsSystem physics(std::move(backend));
    SimulationContext context(registry, 1.0 / 60.0, 0.0);

    physics.tick(context);

    const auto& sense_a = registry.get<ContactSenseComponent>(a);
    const auto& sense_b = registry.get<ContactSenseComponent>(b);
    EXPECT_GT(sense_a.contact_count, 0u);
    EXPECT_GT(sense_b.contact_count, 0u);
    EXPECT_GE(sense_a.contact_force_magnitude, 0.0);
    EXPECT_GE(sense_b.contact_force_magnitude, 0.0);
}

TEST(ContactSense, ZeroWhenNoCollisionOccurs) {
    entt::registry registry;

    const entt::entity a = spawn_body(registry, Vec3{0.0, 3.0, 0.0});
    const entt::entity b = spawn_body(registry, Vec3{5.0, 3.0, 0.0});

    auto backend = std::make_unique<SimplePhysicsBackend>(SimplePhysicsConfig{});
    PhysicsSystem physics(std::move(backend));
    SimulationContext context(registry, 1.0 / 60.0, 0.0);

    physics.tick(context);

    const auto& sense_a = registry.get<ContactSenseComponent>(a);
    const auto& sense_b = registry.get<ContactSenseComponent>(b);
    EXPECT_EQ(sense_a.contact_count, 0u);
    EXPECT_EQ(sense_b.contact_count, 0u);
    EXPECT_EQ(sense_a.contact_force_magnitude, 0.0);
    EXPECT_EQ(sense_b.contact_force_magnitude, 0.0);
}
