#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "evolution/sim/scheduler.h"

namespace evolution::sim {

struct AdaptiveControlConfig {
    bool enabled{true};
    std::size_t target_population_min{300};
    std::size_t target_population_max{1200};
    double target_herbivore_plant_ratio_min{0.03};
    double target_herbivore_plant_ratio_max{0.25};
    double max_control_step_per_sec{0.05};
    double integral_clamp{5.0};
    double cooldown_after_rescue_s{20.0};
};

struct AdaptiveControlState {
    AdaptiveControlConfig config{};
    double plant_seeding_multiplier_override{-1.0};
    double reproduction_density_bias{0.0};
    double rescue_bias{0.0};
    double cull_bias{0.0};

    double population_error_integral{0.0};
    double ratio_error_integral{0.0};
    double rescue_cooldown_remaining{0.0};
    std::uint64_t last_rescues_seen{0};
};

class AdaptiveControlSystem final : public ISystem {
public:
    explicit AdaptiveControlSystem(const AdaptiveControlConfig& config = {}) noexcept
        : config_(config) {}

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "adaptive_control";
    AdaptiveControlConfig config_{};
};

}  // namespace evolution::sim
