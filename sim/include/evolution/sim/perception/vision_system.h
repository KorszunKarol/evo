#pragma once

#include <string_view>

#include "evolution/sim/scheduler.h"

namespace evolution::sim {

class VisionSystem final : public ISystem {
public:
    VisionSystem() = default;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "vision";
};

}  // namespace evolution::sim
