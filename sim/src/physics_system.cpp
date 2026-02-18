#include "evolution/sim/physics_system.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"

namespace evolution::sim {

namespace {

constexpr double kMaxPlanarSpeed = 12.0;
constexpr double kBoundaryVelocityDamping = 0.35;

void enforce_world_bounds(entt::registry& registry) {
    const auto* terrain = registry.ctx().find<Terrain>();
    if (terrain == nullptr) {
        return;
    }

    const double max_x = std::max(0.0, static_cast<double>(terrain->width() - 1) * terrain->cell_size());
    const double max_z =
        std::max(0.0, static_cast<double>(terrain->height_cells() - 1) * terrain->cell_size());

    auto view = registry.view<TransformComponent, KinematicsComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& kinematics = view.get<KinematicsComponent>(entity);

        const bool invalid_position = !std::isfinite(transform.position.x) ||
                                      !std::isfinite(transform.position.y) ||
                                      !std::isfinite(transform.position.z);
        const bool invalid_velocity = !std::isfinite(kinematics.linear_velocity.x) ||
                                      !std::isfinite(kinematics.linear_velocity.y) ||
                                      !std::isfinite(kinematics.linear_velocity.z);
        if (invalid_position || invalid_velocity) {
            const double fallback_x = std::isfinite(transform.position.x) ? transform.position.x : 0.0;
            const double fallback_z = std::isfinite(transform.position.z) ? transform.position.z : 0.0;
            transform.position.x = std::clamp(fallback_x, 0.0, max_x);
            transform.position.z = std::clamp(fallback_z, 0.0, max_z);
            transform.position.y = terrain->height(transform.position.x, transform.position.z);
            kinematics.linear_velocity = Vec3{0.0, 0.0, 0.0};
            kinematics.accumulated_force = Vec3{0.0, 0.0, 0.0};
        }

        bool clamped_x = false;
        bool clamped_z = false;
        if (transform.position.x < 0.0) {
            transform.position.x = 0.0;
            clamped_x = true;
        } else if (transform.position.x > max_x) {
            transform.position.x = max_x;
            clamped_x = true;
        }

        if (transform.position.z < 0.0) {
            transform.position.z = 0.0;
            clamped_z = true;
        } else if (transform.position.z > max_z) {
            transform.position.z = max_z;
            clamped_z = true;
        }

        if (clamped_x) {
            kinematics.linear_velocity.x = -kinematics.linear_velocity.x * kBoundaryVelocityDamping;
        }
        if (clamped_z) {
            kinematics.linear_velocity.z = -kinematics.linear_velocity.z * kBoundaryVelocityDamping;
        }

        const double vx = kinematics.linear_velocity.x;
        const double vz = kinematics.linear_velocity.z;
        const double planar_speed = std::sqrt(vx * vx + vz * vz);
        if (planar_speed > kMaxPlanarSpeed) {
            const double scale = kMaxPlanarSpeed / std::max(planar_speed, 1e-9);
            kinematics.linear_velocity.x *= scale;
            kinematics.linear_velocity.z *= scale;
        }
    }
}

}  // namespace

PhysicsSystem::PhysicsSystem(std::unique_ptr<IPhysicsBackend> backend)
    : backend_(std::move(backend)) {
    if (!backend_) {
        throw std::invalid_argument("PhysicsSystem requires a valid backend instance");
    }
}

void PhysicsSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    backend_->sync_from_registry(registry);
    backend_->step(registry, context.fixed_dt());
    enforce_world_bounds(registry);
    stats_cache_ = backend_->stats();

    auto contact_view = registry.view<ContactSenseComponent>();
    contact_view.each([](ContactSenseComponent& sense) {
        sense.contact_count = 0;
        sense.contact_normal_sum = Vec3{0.0, 0.0, 0.0};
        sense.contact_force_magnitude = 0.0;
    });

    for (const auto& event : backend_->contact_events()) {
        if (!(event.begin || event.stay) || event.end) {
            continue;
        }

        if (auto* sense_a = registry.try_get<ContactSenseComponent>(event.entity_a)) {
            ++sense_a->contact_count;
            sense_a->contact_normal_sum += event.normal;
            sense_a->contact_force_magnitude += event.impulse_magnitude;
        }
        if (auto* sense_b = registry.try_get<ContactSenseComponent>(event.entity_b)) {
            ++sense_b->contact_count;
            sense_b->contact_normal_sum -= event.normal;
            sense_b->contact_force_magnitude += event.impulse_magnitude;
        }
    }

    contact_view.each([](ContactSenseComponent& sense) {
        if (sense.contact_count == 0) {
            return;
        }
        const double inv = 1.0 / static_cast<double>(sense.contact_count);
        sense.contact_normal_sum *= inv;
    });
}

}  // namespace evolution::sim
