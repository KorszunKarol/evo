#include "evolution/sim/adaptive_control_system.h"

#include <algorithm>
#include <cmath>

#include "evolution/sim/population_monitor.h"

namespace evolution::sim {

namespace {

[[nodiscard]] double clamp_unit(double x) noexcept {
    return std::clamp(x, -1.0, 1.0);
}

}  // namespace

void AdaptiveControlSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    if (!registry.ctx().contains<AdaptiveControlState>()) {
        registry.ctx().emplace<AdaptiveControlState>(AdaptiveControlState{.config = config_});
    }

    auto& state = registry.ctx().get<AdaptiveControlState>();
    state.config = config_;

    if (!config_.enabled) {
        state.plant_seeding_multiplier_override = -1.0;
        state.reproduction_density_bias = 0.0;
        state.rescue_bias = 0.0;
        state.cull_bias = 0.0;
        return;
    }

    const auto* monitor = registry.ctx().find<PopulationMonitor>();
    if (monitor == nullptr) {
        return;
    }

    const double dt = context.fixed_dt();

    const double pop = static_cast<double>(monitor->latest.creature_count);
    const double pop_mid = 0.5 * static_cast<double>(config_.target_population_min + config_.target_population_max);
    const double pop_half_range =
        0.5 * std::max(1.0, static_cast<double>(config_.target_population_max - config_.target_population_min));
    const double pop_error = clamp_unit((pop - pop_mid) / pop_half_range);

    const double ratio = monitor->latest.herbivore_to_plant_ratio;
    const double ratio_mid = 0.5 * (config_.target_herbivore_plant_ratio_min + config_.target_herbivore_plant_ratio_max);
    const double ratio_half_range =
        0.5 * std::max(1e-4, config_.target_herbivore_plant_ratio_max - config_.target_herbivore_plant_ratio_min);
    const double ratio_error = clamp_unit((ratio - ratio_mid) / ratio_half_range);

    state.population_error_integral = std::clamp(state.population_error_integral + pop_error * dt,
                                                 -std::abs(config_.integral_clamp),
                                                 std::abs(config_.integral_clamp));
    state.ratio_error_integral = std::clamp(state.ratio_error_integral + ratio_error * dt,
                                            -std::abs(config_.integral_clamp),
                                            std::abs(config_.integral_clamp));

    if (state.rescue_cooldown_remaining > 0.0) {
        state.rescue_cooldown_remaining = std::max(0.0, state.rescue_cooldown_remaining - dt);
    }
    if (const auto* counters = registry.ctx().find<PopulationEventCounters>()) {
        if (counters->rescues_total > state.last_rescues_seen) {
            state.rescue_cooldown_remaining = std::max(0.0, config_.cooldown_after_rescue_s);
            state.last_rescues_seen = counters->rescues_total;
        }
    }

    const double max_step = std::max(0.001, config_.max_control_step_per_sec) * dt;
    const double seeding_target = std::clamp(1.0 - 0.45 * pop_error - 0.35 * ratio_error, 0.05, 1.5);
    const double current_seeding =
        state.plant_seeding_multiplier_override > 0.0 ? state.plant_seeding_multiplier_override : 1.0;
    state.plant_seeding_multiplier_override =
        current_seeding + std::clamp(seeding_target - current_seeding, -max_step, max_step);

    const double cull_target = clamp_unit(0.65 * pop_error + 0.25 * ratio_error + 0.1 * state.population_error_integral);
    state.cull_bias = state.cull_bias + std::clamp(cull_target - state.cull_bias, -max_step, max_step);

    const double rescue_target = clamp_unit(-0.7 * pop_error - 0.2 * ratio_error - 0.1 * state.population_error_integral);
    const double rescue_gate = state.rescue_cooldown_remaining > 0.0 ? 0.35 : 1.0;
    const double gated_target = rescue_target * rescue_gate;
    state.rescue_bias = state.rescue_bias + std::clamp(gated_target - state.rescue_bias, -max_step, max_step);

    const double reproduction_target = clamp_unit(-0.6 * pop_error - 0.4 * ratio_error);
    state.reproduction_density_bias =
        state.reproduction_density_bias + std::clamp(reproduction_target - state.reproduction_density_bias,
                                                     -max_step,
                                                     max_step);
}

}  // namespace evolution::sim
