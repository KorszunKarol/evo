#include "evolution/sim/motor_system.h"

#include <algorithm>
#include <cmath>

namespace evolution::sim {

namespace {

[[nodiscard]] double EstimateOnGround(const TransformComponent& transform) noexcept {
    return (transform.position.y < 0.1) ? 1.0 : 0.0;
}

[[nodiscard]] double ClampEnergy(double value, double max_value) noexcept {
    return std::clamp(value, 0.0, max_value);
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
        
        // Get locomotion traits (defaults if not present)
        double muscle_strength = 1.0;
        double jump_power = 1.0;
        if (const auto* loco = registry.try_get<LocomotionComponent>(entity)) {
            muscle_strength = loco->muscle_strength;
            jump_power = loco->jump_power;
        }
        
        // Apply per-entity muscle strength scaling
        const double effective_impulse = impulse_scale_ * muscle_strength;

        sim::Vec3 planar_force{actuation.impulse_x * effective_impulse, 0.0,
                               actuation.impulse_z * effective_impulse};
        kinematics.accumulated_force += planar_force;

        bool jump_applied = false;
        if (actuation.jump && EstimateOnGround(transform) > 0.5) {
            const double jump_force = jump_impulse_ * jump_power;
            kinematics.accumulated_force += sim::Vec3{0.0, jump_force, 0.0};
            jump_applied = true;
        }

        // Energy cost scales with muscle strength (stronger = costlier)
        const double move_cost = (std::abs(actuation.impulse_x) + std::abs(actuation.impulse_z)) 
                                  * effective_impulse * 0.01;
        const double jump_cost = jump_applied ? (5.0 * jump_power) : 0.0;
        const double total_cost = (move_cost + jump_cost) * context.fixed_dt();

        metabolism.energy = ClampEnergy(metabolism.energy - total_cost, metabolism.max_energy);

        if (auto* telem = registry.try_get<TelemetryComponent>(entity)) {
            telem->total_energy_lost_movement += total_cost;
        }

        if (auto* feeding = registry.try_get<FeedingIntent>(entity)) {
            feeding->request_eat = actuation.eat;
        }

        actuation.impulse_x = 0.0;
        actuation.impulse_z = 0.0;
        actuation.jump = false;
        actuation.eat = false;
        actuation.update_skip = 0;
    }
}

}  // namespace evolution::sim

