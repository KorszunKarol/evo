#include <gtest/gtest.h>

#include "evolution/sim/physics/simple_backend.h"

using namespace evolution::sim;

TEST(SimplePhysicsBackend, SphereBouncesFromGround) {
    entt::registry registry;
    const entt::entity entity = registry.create();

    TransformComponent transform{};
    transform.position = {0.0, 2.0, 0.0};
    registry.emplace<TransformComponent>(entity, transform);

    KinematicsComponent kinematics{};
    kinematics.linear_velocity = {0.0, 0.0, 0.0};
    kinematics.inverse_mass = 1.0;
    kinematics.linear_damping = 0.0;
    kinematics.restitution = 1.0;
    kinematics.friction = 0.0;
    registry.emplace<KinematicsComponent>(entity, kinematics);

    ColliderComponent collider{};
    collider.type = ShapeType::Sphere;
    collider.sphere.radius = 0.5;
    collider.material.restitution = 1.0;
    collider.material.friction = 0.0;
    registry.emplace<ColliderComponent>(entity, collider);

    registry.emplace<RigidbodyComponent>(entity, RigidbodyComponent{});

    SimplePhysicsConfig config{};
    config.gravity = -9.81;
    config.ground_height = 0.0;
    config.core.cell_size = 1.0;
    config.core.solver_iterations = 8;
    config.core.baumgarte = 0.2;
    config.core.penetration_slop = 0.001;

    SimplePhysicsBackend backend(config);

    const double dt = 1.0 / 120.0;
    double max_height = registry.get<TransformComponent>(entity).position.y;
    for (int step = 0; step < 360; ++step) {
        backend.sync_from_registry(registry);
        backend.step(registry, dt);
        const double height = registry.get<TransformComponent>(entity).position.y;
        max_height = std::max(max_height, height);
    }

    const auto& final_kinematics = registry.get<KinematicsComponent>(entity);
    (void)final_kinematics;
    EXPECT_GT(max_height, 1.5);
}


