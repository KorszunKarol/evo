#pragma once

#include <string_view>
#include <vector>

#include <entt/entt.hpp>

#include "evolution/sim/components.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Per-tick decomposition statistics stored in registry context.
 * @param None.
 * @return None.
 * @note Values reset each tick by DecompositionSystem.
 * @warning Context resource is optional; default-initialized when first accessed.
 * @threadsafe Not thread-safe; mutate only within the simulation tick.
 * @complexity O(1).
 * @throws None.
 */
struct DecompositionStatistics {
    double biomass_decayed_last_tick{0.0};
    double nutrients_returned_last_tick{0.0};
};

/**
 * @brief Processes corpse decay and recycling of biomass into soil nutrients.
 *
 * @details This system iterates over all entities with a CorpseComponent, reduces their
 * biomass over time, and injects a corresponding amount of nitrogen into the soil at
 * the corpse's location. When biomass reaches zero, the corpse entity is destroyed.
 */
class DecompositionSystem final : public ISystem {
public:
    explicit DecompositionSystem() noexcept = default;

    /**
     * @brief Updates all corpses and modifies soil nutrients.
     */
    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "decomposition";
    std::vector<entt::entity> removal_queue_{};
};

}  // namespace evolution::sim
