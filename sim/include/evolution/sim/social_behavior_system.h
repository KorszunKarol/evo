#pragma once

#include <string_view>
#include <vector>

#include "evolution/sim/components.h"
#include "evolution/sim/creature_spatial_index.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Computes brain-driven social signals (flocking, territoriality, pack hunting).
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(N * K) where K is average neighbor count within query radii.
 * @note Writes SocialSignalsComponent each tick for alive creatures.
 * @warning Requires CreatureSpatialIndex to be rebuilt before running.
 * @threadsafe @notthreadsafe.
 */
class SocialBehaviorSystem final : public ISystem {
public:
    /**
     * @brief Constructs the system with default tuning constants.
     * @param None.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Uses fixed constants for neighbor and territory queries.
     * @warning Constants are currently compile-time; adjust carefully.
     * @threadsafe @notthreadsafe.
     */
    SocialBehaviorSystem() noexcept = default;

    /**
     * @brief Computes social signals for all eligible creatures.
     * @param context SimulationContext& Provides registry access and timing metadata.
     * @return None.
     * @throws None.
     * @complexity O(N * K) where K is average neighbor count within query radii.
     * @note Requires a valid CreatureSpatialIndex in registry context.
     * @warning Must run before BrainInferenceSystem.
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
    static constexpr std::string_view name_ = "social_behavior";
    static constexpr double kNeighborRadius = 10.0;
    static constexpr double kSeparationRadius = 3.0;
    static constexpr double kTerritoryRadius = 12.0;
    static constexpr double kMaxPreyRadius = 20.0;
    static constexpr double kNearPreyRadius = 6.0;
    static constexpr double kMaxNeighborsForDensity = 8.0;
    static constexpr double kMaxIntrudersForDensity = 4.0;
    static constexpr double kMaxPackForDensity = 4.0;

    std::vector<entt::entity> scratch_neighbors_{};
};

}  // namespace evolution::sim
