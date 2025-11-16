#pragma once

#include <optional>

#include <entt/entt.hpp>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/physics/physics_types.h"

namespace evolution::sim {

/**
 * @brief Populates a contact manifold for sphere-sphere collisions.
 *
 * @param entity_a entt::entity First entity.
 * @param entity_b entt::entity Second entity.
 * @param center_a const Vec3& Sphere A center.
 * @param radius_a double Sphere A radius.
 * @param center_b const Vec3& Sphere B center.
 * @param radius_b double Sphere B radius.
 * @param[out] manifold ContactManifold& Output manifold to populate.
 * @return bool True when a penetrating contact exists.
 */
bool collide_sphere_sphere(entt::entity entity_a,
                           entt::entity entity_b,
                           const Vec3& center_a,
                           double radius_a,
                           const Vec3& center_b,
                           double radius_b,
                           ContactManifold& manifold);

/**
 * @brief Sphere vs axis-aligned box overlap test.
 *
 * @param entity_a entt::entity Sphere entity.
 * @param entity_b entt::entity Box entity.
 * @param center const Vec3& Sphere center.
 * @param radius double Sphere radius.
 * @param box_min const Vec3& Box minimum corner.
 * @param box_max const Vec3& Box maximum corner.
 * @param[out] manifold ContactManifold& Output manifold.
 * @return bool True when an intersection is detected.
 */
bool collide_sphere_aabb(entt::entity entity_a,
                         entt::entity entity_b,
                         const Vec3& center,
                         double radius,
                         const Vec3& box_min,
                         const Vec3& box_max,
                         ContactManifold& manifold);

/**
 * @brief AABB vs AABB overlap test.
 *
 * @param entity_a entt::entity First entity.
 * @param entity_b entt::entity Second entity.
 * @param min_a const Vec3& First AABB minimum corner.
 * @param max_a const Vec3& First AABB maximum corner.
 * @param min_b const Vec3& Second AABB minimum corner.
 * @param max_b const Vec3& Second AABB maximum corner.
 * @param[out] manifold ContactManifold& Output manifold.
 * @return bool True when intersection occurs.
 */
bool collide_aabb_aabb(entt::entity entity_a,
                       entt::entity entity_b,
                       const Vec3& min_a,
                       const Vec3& max_a,
                       const Vec3& min_b,
                       const Vec3& max_b,
                       ContactManifold& manifold);

/**
 * @brief Sphere vs upright capsule (Y axis) overlap test.
 *
 * @param entity_a entt::entity Sphere entity identifier.
 * @param entity_b entt::entity Capsule entity identifier.
 * @param sphere_center const Vec3& Sphere center.
 * @param sphere_radius double Sphere radius.
 * @param capsule_center const Vec3& Capsule center.
 * @param capsule_radius double Capsule radius.
 * @param capsule_half_height double Capsule half height.
 * @param[out] manifold ContactManifold& Output manifold.
 * @return bool True when intersection occurs.
 */
bool collide_sphere_capsule_y(entt::entity entity_a,
                              entt::entity entity_b,
                              const Vec3& sphere_center,
                              double sphere_radius,
                              const Vec3& capsule_center,
                              double capsule_radius,
                              double capsule_half_height,
                              ContactManifold& manifold);

/**
 * @brief Capsule vs capsule (upright) overlap test.
 *
 * @param entity_a entt::entity First entity.
 * @param entity_b entt::entity Second entity.
 * @param center_a const Vec3& First capsule center.
 * @param radius_a double First capsule radius.
 * @param half_height_a double First capsule half height.
 * @param center_b const Vec3& Second capsule center.
 * @param radius_b double Second capsule radius.
 * @param half_height_b double Second capsule half height.
 * @param[out] manifold ContactManifold& Output manifold.
 * @return bool True when intersection occurs.
 */
bool collide_capsule_y_capsule_y(entt::entity entity_a,
                                 entt::entity entity_b,
                                 const Vec3& center_a,
                                 double radius_a,
                                 double half_height_a,
                                 const Vec3& center_b,
                                 double radius_b,
                                 double half_height_b,
                                 ContactManifold& manifold);

/**
 * @brief Generates contacts against the ground surface (plane or heightfield).
 *
 * @param entity entt::entity Entity to test.
 * @param collider const ColliderComponent& Collider definition.
 * @param position const Vec3& Entity position.
 * @param ground_y double Ground height.
 * @param terrain const Terrain* Optional terrain heightfield; when null a plane is used.
 * @param[out] manifold ContactManifold& Output manifold.
 * @return bool True when the collider penetrates the ground plane.
 */
bool collide_with_ground(entt::entity entity,
                         const ColliderComponent& collider,
                         const Vec3& position,
                         double ground_y,
                         const Terrain* terrain,
                         ContactManifold& manifold);

}  // namespace evolution::sim


