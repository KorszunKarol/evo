#pragma once

#include <array>
#include <cstddef>

#include <entt/entt.hpp>

#include "evolution/sim/math_types.h"

namespace evolution::sim {

/**
 * @brief Represents a single contact constraint between two bodies.
 *
 * @note Penetration depth is clamped to non-negative values to maintain stability.
 */
struct ContactPoint {
    Vec3 position{0.0, 0.0, 0.0};   ///< World-space contact position.
    Vec3 normal{0.0, 1.0, 0.0};      ///< Normal pointing from entity A to entity B.
    double penetration{0.0};         ///< Overlap distance in meters.
    double normal_impulse{0.0};      ///< Accumulated normal impulse (warmstarting).
    double tangent_impulse[2]{0.0, 0.0};  ///< Accumulated friction impulses along orthogonal tangents.
};

/**
 * @brief Aggregates contact points generated for an entity pair.
 *
 * @details Manifolds can be reused across solver iterations; they store per-point impulses for warmstarting.
 */
struct ContactManifold {
    entt::entity entity_a{entt::null};               ///< First participant entity.
    entt::entity entity_b{entt::null};               ///< Second participant entity.
    std::array<ContactPoint, 4> points{};            ///< Contact cache (up to 4 points).
    std::size_t count{0};                            ///< Active contact point count.
    double mixed_friction{0.6};                      ///< Effective friction coefficient.
    double mixed_restitution{0.0};                   ///< Effective restitution coefficient.
    bool is_trigger{false};                          ///< True when manifold should not resolve impulses.
};

}  // namespace evolution::sim


