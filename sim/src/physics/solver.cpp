#include "evolution/sim/physics/solver.h"

#include <algorithm>
#include <cmath>

#include "evolution/sim/components.h"

namespace evolution::sim {

namespace {

constexpr double kEpsilon = 1.0e-6;

[[nodiscard]] double dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

[[nodiscard]] Vec3 normalize_safe(const Vec3& v, const Vec3& fallback = Vec3{1.0, 0.0, 0.0}) noexcept {
    const double len_sq = dot(v, v);
    if (len_sq <= kEpsilon) {
        return fallback;
    }
    const double inv_len = 1.0 / std::sqrt(len_sq);
    return Vec3{v.x * inv_len, v.y * inv_len, v.z * inv_len};
}

[[nodiscard]] Vec3 make_tangent(const Vec3& normal) noexcept {
    const Vec3 axis = (std::abs(normal.y) < 0.999) ? Vec3{0.0, 1.0, 0.0} : Vec3{1.0, 0.0, 0.0};
    return normalize_safe(cross(axis, normal));
}

struct BodyAccess {
    TransformComponent* transform{nullptr};
    KinematicsComponent* kinematics{nullptr};
    RigidbodyComponent* body{nullptr};

    [[nodiscard]] bool is_dynamic() const noexcept {
        if (!kinematics) {
            return false;
        }
        if (!body) {
            return kinematics->inverse_mass > 0.0;
        }
        if (body->is_static || body->is_kinematic) {
            return false;
        }
        return kinematics->inverse_mass > 0.0;
    }

    [[nodiscard]] double inverse_mass() const noexcept {
        return is_dynamic() ? kinematics->inverse_mass : 0.0;
    }
};

BodyAccess fetch_body(entt::registry& registry, entt::entity entity) {
    BodyAccess access{};
    if (entity == entt::null) {
        return access;
    }
    access.transform = registry.try_get<TransformComponent>(entity);
    access.kinematics = registry.try_get<KinematicsComponent>(entity);
    access.body = registry.try_get<RigidbodyComponent>(entity);
    return access;
}

}  // namespace

void solve_velocity_constraints(std::span<ContactManifold> manifolds,
                                entt::registry& registry,
                                int iterations,
                                double dt) {
    (void)dt;  // Currently unused but retained for future mass scaling.
    for (int iteration = 0; iteration < std::max(iterations, 0); ++iteration) {
        for (auto& manifold : manifolds) {
            if (manifold.is_trigger || manifold.count == 0) {
                continue;
            }

            BodyAccess body_a = fetch_body(registry, manifold.entity_a);
            BodyAccess body_b = fetch_body(registry, manifold.entity_b);
            if (!body_a.kinematics && !body_b.kinematics) {
                continue;
            }

            for (std::size_t i = 0; i < manifold.count; ++i) {
                auto& cp = manifold.points[i];
                const Vec3 normal = cp.normal;
                Vec3 tangent1 = make_tangent(normal);
                Vec3 tangent2 = cross(normal, tangent1);

                Vec3* vel_a_ptr = body_a.kinematics ? &body_a.kinematics->linear_velocity : nullptr;
                Vec3* vel_b_ptr = body_b.kinematics ? &body_b.kinematics->linear_velocity : nullptr;
                const Vec3 vel_a = vel_a_ptr ? *vel_a_ptr : Vec3{0.0, 0.0, 0.0};
                const Vec3 vel_b = vel_b_ptr ? *vel_b_ptr : Vec3{0.0, 0.0, 0.0};
                Vec3 relative = vel_b - vel_a;

                const double inv_mass_a = body_a.inverse_mass();
                const double inv_mass_b = body_b.inverse_mass();
                const double inv_mass_sum = inv_mass_a + inv_mass_b;
                if (inv_mass_sum <= kEpsilon) {
                    continue;
                }

                // Normal impulse
                const double vel_along_normal = dot(relative, normal);
                const double bias_velocity = manifold.mixed_restitution * std::min(vel_along_normal, 0.0);
                double impulse = -(vel_along_normal - bias_velocity) / inv_mass_sum;
                const double old_impulse = cp.normal_impulse;
                cp.normal_impulse = std::max(old_impulse + impulse, 0.0);
                impulse = cp.normal_impulse - old_impulse;

                const Vec3 impulse_vector = normal * impulse;
                if (body_a.is_dynamic() && vel_a_ptr) {
                    *vel_a_ptr -= impulse_vector * inv_mass_a;
                }
                if (body_b.is_dynamic() && vel_b_ptr) {
                    *vel_b_ptr += impulse_vector * inv_mass_b;
                }

                // Tangential impulses (friction)
                const Vec3 updated_vel_a = vel_a_ptr ? *vel_a_ptr : Vec3{0.0, 0.0, 0.0};
                const Vec3 updated_vel_b = vel_b_ptr ? *vel_b_ptr : Vec3{0.0, 0.0, 0.0};
                relative = updated_vel_b - updated_vel_a;
                const double max_friction = manifold.mixed_friction * cp.normal_impulse;
                const double vt1 = dot(relative, tangent1);
                double jt1 = -vt1 / inv_mass_sum;
                const double old_tangent1 = cp.tangent_impulse[0];
                cp.tangent_impulse[0] = std::clamp(old_tangent1 + jt1, -max_friction, max_friction);
                jt1 = cp.tangent_impulse[0] - old_tangent1;

                const Vec3 friction_impulse1 = tangent1 * jt1;
                if (body_a.is_dynamic() && vel_a_ptr) {
                    *vel_a_ptr -= friction_impulse1 * inv_mass_a;
                }
                if (body_b.is_dynamic() && vel_b_ptr) {
                    *vel_b_ptr += friction_impulse1 * inv_mass_b;
                }

                const double vt2 = dot(relative, tangent2);
                double jt2 = -vt2 / inv_mass_sum;
                const double old_tangent2 = cp.tangent_impulse[1];
                cp.tangent_impulse[1] = std::clamp(old_tangent2 + jt2, -max_friction, max_friction);
                jt2 = cp.tangent_impulse[1] - old_tangent2;

                const Vec3 friction_impulse2 = tangent2 * jt2;
                if (body_a.is_dynamic() && vel_a_ptr) {
                    *vel_a_ptr -= friction_impulse2 * inv_mass_a;
                }
                if (body_b.is_dynamic() && vel_b_ptr) {
                    *vel_b_ptr += friction_impulse2 * inv_mass_b;
                }
            }
        }
    }
}

void solve_position_constraints(std::span<ContactManifold> manifolds,
                                entt::registry& registry,
                                double baumgarte,
                                double slop) {
    for (auto& manifold : manifolds) {
        if (manifold.is_trigger || manifold.count == 0) {
            continue;
        }

        BodyAccess body_a = fetch_body(registry, manifold.entity_a);
        BodyAccess body_b = fetch_body(registry, manifold.entity_b);
        if (!body_a.transform && !body_b.transform) {
            continue;
        }
        const double inv_mass_a = body_a.inverse_mass();
        const double inv_mass_b = body_b.inverse_mass();
        const double inv_mass_sum = inv_mass_a + inv_mass_b;
        if (inv_mass_sum <= kEpsilon) {
            continue;
        }

        for (std::size_t i = 0; i < manifold.count; ++i) {
            const auto& cp = manifold.points[i];
            const double penetration = cp.penetration;
            if (penetration <= slop) {
                continue;
            }
            const double correction_mag = baumgarte * (penetration - slop) / inv_mass_sum;
            const Vec3 correction = cp.normal * correction_mag;

            if (body_a.transform && body_a.is_dynamic()) {
                body_a.transform->position -= correction * inv_mass_a;
            }
            if (body_b.transform && body_b.is_dynamic()) {
                body_b.transform->position += correction * inv_mass_b;
            }
        }
    }
}

}  // namespace evolution::sim


