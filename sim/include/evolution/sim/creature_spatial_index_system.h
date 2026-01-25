#pragma once

#include <string_view>

#include "evolution/sim/creature_spatial_index.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Rebuilds the CreatureSpatialIndex each tick.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(N) where N is the number of indexed entities.
 * @note Stores the index in registry context for downstream systems.
 * @warning Must run before any system that queries the creature spatial index.
 * @threadsafe @notthreadsafe.
 */
class CreatureSpatialIndexSystem final : public ISystem {
public:
    /**
     * @brief Constructs the system with a configured cell size.
     * @param cell_size Cell size for the spatial hash grid.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Smaller cells improve query precision at higher memory cost.
     * @warning Cell size must be positive.
     * @threadsafe @notthreadsafe.
     */
    explicit CreatureSpatialIndexSystem(double cell_size = 5.0) noexcept;

    /**
     * @brief Rebuilds the spatial index from the current registry state.
     * @param context SimulationContext& Provides registry access and timing metadata.
     * @return None.
     * @throws None.
     * @complexity O(N) where N is the number of indexed entities.
     * @note Stores the index in registry context for downstream systems.
     * @warning Should run once per tick for determinism.
     * @threadsafe @notthreadsafe.
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Human-readable identifier for diagnostics.
     * @param None.
     * @return std::string_view Static string literal naming the system.
     * @throws None.
     * @complexity O(1).
     * @threadsafe Thread-safe as long as the instance outlives the call.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "creature_spatial_index";
    double cell_size_{5.0};
};

}  // namespace evolution::sim
