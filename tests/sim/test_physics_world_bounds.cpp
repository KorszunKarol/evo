#include <memory>
#include <cmath>

#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/physics/simple_backend.h"
#include "evolution/sim/physics_system.h"
#include "evolution/sim/simulation_context.h"

using namespace evolution::sim;

TEST(PhysicsWorldBounds, ClampsDynamicBodyToTerrainDomainAndCapsPlanarSpeed) {
    entt::registry registry;

    TerrainConfig terrain_config{};
    terrain_config.width_cells = 32;
    terrain_config.height_cells = 32;
    terrain_config.cell_size = 2.0;
    terrain_config.elevation_scale = 0.0;
    terrain_config.seed = 7;
    registry.ctx().emplace<Terrain>(terrain_config);

    const entt::entity entity = registry.create();
    registry.emplace<TransformComponent>(entity, TransformComponent{Vec3{200.0, 3.0, -50.0}});
    registry.emplace<KinematicsComponent>(entity,
                                          KinematicsComponent{
                                              .linear_velocity = Vec3{120.0, 0.0, -80.0},
                                              .accumulated_force = Vec3{0.0, 0.0, 0.0},
                                              .inverse_mass = 1.0,
                                              .linear_damping = 0.0,
                                          });
    ColliderComponent collider{};
    collider.type = ShapeType::Sphere;
    collider.sphere.radius = 0.6;
    registry.emplace<ColliderComponent>(entity, collider);
    registry.emplace<RigidbodyComponent>(entity);

    auto backend = std::make_unique<SimplePhysicsBackend>(SimplePhysicsConfig{});
    PhysicsSystem physics(std::move(backend));
    SimulationContext context(registry, 1.0 / 60.0, 0.0);

    physics.tick(context);

    const auto& terrain = registry.ctx().get<Terrain>();
    const auto& transform = registry.get<TransformComponent>(entity);
    const auto& kinematics = registry.get<KinematicsComponent>(entity);

    const double max_x = static_cast<double>(terrain.width() - 1) * terrain.cell_size();
    const double max_z = static_cast<double>(terrain.height_cells() - 1) * terrain.cell_size();
    EXPECT_GE(transform.position.x, 0.0);
    EXPECT_LE(transform.position.x, max_x);
    EXPECT_GE(transform.position.z, 0.0);
    EXPECT_LE(transform.position.z, max_z);

    const double planar_speed = std::sqrt(kinematics.linear_velocity.x * kinematics.linear_velocity.x +
                                          kinematics.linear_velocity.z * kinematics.linear_velocity.z);
    EXPECT_LE(planar_speed, 12.000001);
}
