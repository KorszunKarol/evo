#include "evolution/sim/motor_system.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "evolution/sim/telemetry_system.h"

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
    auto* telemetry_ctx = registry.ctx().find<TelemetryContext>();
    TelemetrySystem* telemetry = telemetry_ctx != nullptr ? telemetry_ctx->system : nullptr;
    auto view = registry.view<ActuationComponent,
                              KinematicsComponent,
                              MetabolismComponent,
                              TransformComponent>();

    for (auto entity : view) {
        auto& actuation = view.get<ActuationComponent>(entity);
        auto& kinematics = view.get<KinematicsComponent>(entity);
        auto& metabolism = view.get<MetabolismComponent>(entity);
        const auto& transform = view.get<TransformComponent>(entity);

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
            feeding->request_eat = actuation.eat;
        }

        if (telemetry != nullptr) {
            const auto* genome = registry.try_get<GenomeHandleComponent>(entity);
            const genetics::GenomeId genome_id = genome != nullptr ? genome->id : 0;
            const bool force_capture = telemetry->should_capture(entity, 0, genome_id);

            std::ostringstream payload;
            payload << "{"
                    << "\"entity_id\":" << static_cast<std::uint32_t>(entity)
                    << ",\"genome_id\":" << genome_id
                    << ",\"impulse_x\":" << actuation.impulse_x
                    << ",\"impulse_z\":" << actuation.impulse_z
                    << ",\"jump_requested\":" << (actuation.jump ? "true" : "false")
                    << ",\"jump_applied\":" << (jump_applied ? "true" : "false")
                    << ",\"force_x\":" << planar_force.x
                    << ",\"force_y\":" << planar_force.y
                    << ",\"force_z\":" << planar_force.z
                    << ",\"energy_cost\":" << total_cost
                    << "}";

            TelemetryEvent event{
                TelemetryEventType::ACTUATION_APPLIED,
                context.simulation_time(),
                payload.str()
            };
            telemetry->emit_event(event, force_capture);
        }

        actuation.impulse_x = 0.0;
        actuation.impulse_z = 0.0;
        actuation.jump = false;
        actuation.eat = false;
        actuation.update_skip = 0;
    }
}

}  // namespace evolution::sim
