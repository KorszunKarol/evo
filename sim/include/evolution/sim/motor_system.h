/**
 * @file motor_system.h
 * @brief Converts brain actuation commands into physics impulses.
 */

#pragma once

#include <string_view>

#include "evolution/sim/components.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Applies forces/impulses based on brain outputs and charges metabolic cost.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity Construction is O(1).
 * @note Designed to run immediately after BrainInferenceSystem.
 * @warning Not thread-safe; operates on mutable ECS state.
 * @threadsafe @notthreadsafe.
 */
class MotorSystem final : public ISystem {
public:
    /**
     * @brief Configure scaling coefficients for planar and jump thrust.
     * @param impulse_scale Baseline Newton force applied for unit actuation.
     * @param jump_impulse Vertical impulse magnitude for jump command.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Values tuned for prototype; adjust for balancing.
     * @warning Excessive magnitudes may destabilize physics integration.
     * @threadsafe @notthreadsafe.
     */
    explicit MotorSystem(double impulse_scale = 150.0, double jump_impulse = 250.0) noexcept;

    /**
     * @brief Apply forces derived from ActuationComponent and deduct energy cost.
     * @param context SimulationContext& Supplies registry and timestep.
     * @return None.
     * @throws None.
     * @complexity O(N) where N equals number of active actuators.
     * @note Resets actuation commands after application.
     * @warning Requires single-thread execution due to shared registry mutation.
     * @threadsafe @notthreadsafe.
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Human-readable system label.
     * @param None.
     * @return std::string_view Constant literal.
     * @throws None.
     * @complexity O(1).
     * @note Used by diagnostics/logging.
     * @warning None.
     * @threadsafe Thread-safe for concurrent reads.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "motor";
    double impulse_scale_{150.0};
    double jump_impulse_{250.0};
};

}  // namespace evolution::sim


