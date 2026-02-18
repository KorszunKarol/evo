#pragma once

#include <string_view>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Transfers energy from nearby plants to herbivores requesting feeding.
 */
class FeedingSystem final : public ISystem {
public:
    struct Tuning {
        bool enable_debug{true};
        double debug_interval_s{1.0};
        double predation_damage_scale{1.0};
        double predation_conversion_efficiency_scale{1.0};
        double pursuit_timeout_scale{1.0};
        double attack_cooldown_scale{1.0};
    };

    FeedingSystem() noexcept : FeedingSystem(Tuning{}) {}

    explicit FeedingSystem(Tuning tuning) noexcept
        : tuning_(tuning),
          debug_enabled_(tuning.enable_debug),
          debug_interval_s_(tuning.debug_interval_s) {}

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "feeding";
    Tuning tuning_{};
    bool debug_enabled_{true};
    double debug_interval_s_{1.0};
    double debug_accumulator_{0.0};
};

}  // namespace evolution::sim
