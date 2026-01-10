#pragma once

#include <span>

#include <entt/entt.hpp>

#include "evolution/sim/physics/physics_types.h"

namespace evolution::sim {

/**
 * @brief Resolves contact constraints using sequential impulses.
 *
 * @param manifolds std::span<ContactManifold> Mutable manifolds to process.
 * @param registry entt::registry& Registry containing transforms and kinematics.
 * @param iterations int Solver iteration count.
 * @param dt double Simulation timestep.
 * @note Warmstarting fields stored in manifolds are used and updated in-place.
 * @complexity O(M * iterations) where M is contact point count.
 */
void solve_velocity_constraints(std::span<ContactManifold> manifolds,
                                entt::registry& registry,
                                int iterations,
                                double dt);

/**
 * @brief Applies Baumgarte positional correction to reduce interpenetration.
 *
 * @param manifolds std::span<ContactManifold> Contact manifolds to correct.
 * @param registry entt::registry& Registry to modify entity transforms.
 * @param baumgarte double Correction factor β in [0,1].
 * @param slop double Penetration slop before applying correction (meters).
 * @complexity O(M) where M is contact point count.
 */
void solve_position_constraints(std::span<ContactManifold> manifolds,
                                entt::registry& registry,
                                double baumgarte,
                                double slop);

/**
 * @brief Resolves joint constraints (hinges, fixed, spherical).
 *
 * @param registry entt::registry& Registry containing joints and bodies.
 * @param dt double Simulation timestep.
 * @complexity O(J) where J is joint count.
 */
void solve_joint_constraints(entt::registry& registry, double dt);

}  // namespace evolution::sim


