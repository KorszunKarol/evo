#pragma once

#include <string_view>
#include <vector>

#include <entt/entt.hpp>

#include "evolution/sim/components.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Applies basal metabolic drain and optionally removes exhausted entities.
 *
 * @details The system iterates over all entities carrying a MetabolismComponent, consumes
 * energy based on the configured basal_rate and current fixed timestep, clamps the result
 * to the component's valid range, and queues entities whose stores reach zero for destruction.
 * Destruction is deferred until the end of the tick pass to avoid invalidating iterators.
 *
 * @note Designed as a lightweight life-support loop that keeps the population bounded while
 * still allowing other subsystems (feeding, reproduction) to inject or consume energy.
 * @notthreadsafe Must be invoked exclusively on the simulation thread.
 */
class MetabolismSystem final : public ISystem {
public:
    /**
     * @brief Constructs the system with an optional kill switch for depleted entities.
     *
     * @param destroy_on_zero bool When true, entities with non-positive energy are destroyed after processing.
     * @throws None This constructor does not throw.
     * @complexity O(1)
     * @threadsafe @notthreadsafe Construction is thread-safe, but typical usage occurs on the main simulation thread.
     * @example
     * MetabolismSystem metabolism;
     * app.scheduler().add_system(std::make_unique<MetabolismSystem>(metabolism));
     */
    explicit MetabolismSystem(bool destroy_on_zero = true) noexcept;

    /**
     * @brief Consumes energy reserves and optionally despawns entities that run out.
     *
     * @param context SimulationContext& Provides registry access and timing metadata.
     * @throws None No exceptions are thrown during normal operation.
     * @complexity O(N) where N is the number of entities with MetabolismComponent.
     * @warning Entities removed during this pass are destroyed at the end of the tick to preserve iterator validity.
     * @threadsafe @notthreadsafe Invocation must remain on the simulation thread because shared ECS state is mutated.
     * @example
     * SimulationContext ctx{registry, dt, time};
     * metabolism.tick(ctx);
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Human-readable identifier for diagnostics.
     *
     * @return std::string_view Static string literal naming the system.
     * @throws None No exceptions are thrown.
     * @complexity O(1)
     * @threadsafe Thread-safe as long as the instance outlives the call.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    /**
     * @brief Configures whether entities should be destroyed when energy is depleted.
     *
     * @param enabled bool Flag indicating whether hard depletion should trigger destruction.
     * @throws None No exceptions are thrown.
     * @complexity O(1)
     * @threadsafe @notthreadsafe Should be called on the simulation thread during setup.
     */
    void set_destroy_on_zero(bool enabled) noexcept { destroy_on_zero_ = enabled; }

private:
    static constexpr std::string_view name_ = "metabolism";
    bool destroy_on_zero_{true};
    std::vector<entt::entity> recycle_bin_{};
};

}  // namespace evolution::sim


