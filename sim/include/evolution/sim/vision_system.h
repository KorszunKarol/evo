#pragma once

#include "evolution/sim/physics/backend.h"
#include "evolution/sim/scheduler.h"
#include "evolution/sim/simulation_context.h"

namespace evolution::sim {

/**
 * @brief Updates VisionComponent for entities by casting rays into the physics scene.
 *
 * @note Must execute after PhysicsSystem (to have current body positions) and before
 *       BrainInferenceSystem (so brains can consume vision data).
 */
class VisionSystem final : public ISystem {
public:
    /**
     * @brief Constructs the vision system with a reference to the physics backend.
     *
     * @param backend const IPhysicsBackend& Reference to the physics backend for raycasting.
     */
    explicit VisionSystem(const IPhysicsBackend& backend) noexcept;

    /**
     * @brief Updates vision data for all entities with VisionComponent.
     *
     * @param context SimulationContext& Simulation context providing registry access.
     */
    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "vision";
    const IPhysicsBackend& backend_;
};

}  // namespace evolution::sim
