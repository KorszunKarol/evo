#include "evolution/sim/motor_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "evolution/sim/environment/environment.h"

namespace evolution::sim {

namespace {

[[nodiscard]] double EstimateOnGround(const TransformComponent& transform) noexcept {
    return (transform.position.y < 0.1) ? 1.0 : 0.0;
}

[[nodiscard]] double ClampEnergy(double value, double max_value) noexcept {
    return std::clamp(value, 0.0, max_value);
}

[[nodiscard]] double planar_length_sq(const Vec3& v) noexcept {
    return v.x * v.x + v.z * v.z;
}

}  // namespace

MotorSystem::MotorSystem(double impulse_scale, double jump_impulse) noexcept
    : impulse_scale_(impulse_scale), jump_impulse_(jump_impulse) {}

void MotorSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto view = registry.view<ActuationComponent,
                              KinematicsComponent,
                              MetabolismComponent,
                              TransformComponent>();

    for (auto entity : view) {
        auto& actuation = view.get<ActuationComponent>(entity);
        auto& kinematics = view.get<KinematicsComponent>(entity);
        auto& metabolism = view.get<MetabolismComponent>(entity);
        const auto& transform = view.get<TransformComponent>(entity);

        // Continuous survival fallback: hungry herbivores steer directly toward
        // nearest plants each tick, independent of brain update rate.
        if (const auto* diet = registry.try_get<DietComponent>(entity);
            diet != nullptr && diet->type == DietType::Herbivore) {
            if (auto* feeding = registry.try_get<FeedingIntent>(entity);
                feeding != nullptr && metabolism.energy + 1e-6 < metabolism.max_energy) {
                if (const auto* plant_index = registry.ctx().find<PlantSpatialIndex>();
                    plant_index != nullptr) {
                    entt::entity nearest_plant = entt::null;
                    double nearest_dist_sq = std::numeric_limits<double>::max();
                    constexpr double kForageRadius = 96.0;
                    plant_index->for_each_in_radius(
                        registry,
                        transform.position,
                        kForageRadius,
                        [&](entt::entity plant_entity, double dist_sq) {
                            auto* plant = registry.try_get<PlantComponent>(plant_entity);
                            if (plant == nullptr || !plant->alive) {
                                return;
                            }
                            if (dist_sq < nearest_dist_sq) {
                                nearest_dist_sq = dist_sq;
                                nearest_plant = plant_entity;
                            }
                        });

                    if (nearest_plant != entt::null) {
                        const auto* plant_transform = registry.try_get<TransformComponent>(nearest_plant);
                        const auto* plant = registry.try_get<PlantComponent>(nearest_plant);
                        if (plant_transform != nullptr && plant != nullptr) {
                            const Vec3 delta = plant_transform->position - transform.position;
                            const double len_sq = planar_length_sq(delta);
                            const double len = std::sqrt(std::max(0.0, len_sq));
                            const double contact_distance = feeding->reach + plant->radius + 0.2;
                            if (len <= contact_distance) {
                                actuation.impulse_x = 0.0;
                                actuation.impulse_z = 0.0;
                                // Damp planar speed to reduce overshoot and "orbiting" away.
                                kinematics.linear_velocity.x *= 0.55;
                                kinematics.linear_velocity.z *= 0.55;
                            } else if (len_sq > 1e-9) {
                                const double inv_len = 1.0 / std::sqrt(len_sq);
                                actuation.impulse_x = delta.x * inv_len;
                                actuation.impulse_z = delta.z * inv_len;
                            }
                            actuation.eat = true;
                        }
                    }
                }
            }
        }

        const double inverse_mass = kinematics.inverse_mass;
        const double mass = (inverse_mass > 1e-8) ? 1.0 / inverse_mass : 0.0;

        sim::Vec3 planar_force{actuation.impulse_x * impulse_scale_ * mass, 0.0,
                               actuation.impulse_z * impulse_scale_ * mass};
        kinematics.accumulated_force += planar_force;

        bool jump_applied = false;
        if (actuation.jump && EstimateOnGround(transform) > 0.5) {
            const double jump_force = jump_impulse_ * mass;
            kinematics.accumulated_force += sim::Vec3{0.0, jump_force, 0.0};
            jump_applied = true;
        }

        const double planar_magnitude = std::sqrt(planar_force.x * planar_force.x +
                                                  planar_force.z * planar_force.z);
        const double move_cost = planar_magnitude * 0.01;
        const double jump_cost = jump_applied ? 5.0 : 0.0;
        const double total_cost = (move_cost + jump_cost) * context.fixed_dt();

        metabolism.energy = ClampEnergy(metabolism.energy - total_cost, metabolism.max_energy);

        if (auto* feeding = registry.try_get<FeedingIntent>(entity)) {
            // Keep feeding enabled by default; explicit eat output can only enable it.
            feeding->request_eat = feeding->request_eat || actuation.eat;
        }

        actuation.impulse_x = 0.0;
        actuation.impulse_z = 0.0;
        actuation.jump = false;
        actuation.eat = false;
        actuation.update_skip = 0;
    }
}

}  // namespace evolution::sim
