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
    FeedingSystem() = default;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "feeding";
};

}  // namespace evolution::sim



