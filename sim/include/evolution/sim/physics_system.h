#pragma once

#include <memory>
#include <string_view>

#include "evolution/sim/physics/backend.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief System façade that delegates simulation to a pluggable physics backend.
 *
 * @details The system owns an implementation of IPhysicsBackend and forwards tick invocations,
 * propagating ECS state through the backend interface. It is responsible for lazily syncing
 * registry data and exposing backend statistics to diagnostics subsystems.
 *
 * @warning Instances are not thread-safe; invoke tick only from the simulation thread.
 * @threadsafe @notthreadsafe
 */
class PhysicsSystem final : public ISystem {
public:
    /**
     * @brief Constructs the system with a backend implementation.
     *
     * @param backend std::unique_ptr<IPhysicsBackend> Ownership-transferred backend instance.
     * @throws std::invalid_argument if backend is nullptr.
     */
    explicit PhysicsSystem(std::unique_ptr<IPhysicsBackend> backend);

    /**
     * @brief Advances the physics simulation by one fixed timestep.
     *
     * @param context SimulationContext& Provides registry access and timing metadata.
     * @note The backend is synchronized with the registry state before stepping.
     * @complexity O(N + P) where N is body count and P is contact pair count.
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Human-readable identifier reported to the scheduler.
     *
     * @return std::string_view Static literal name.
     */
    [[nodiscard]] std::string_view name() const override { return name_; }

    /**
     * @brief Retrieves read-only backend statistics from the previous tick.
     *
     * @return IPhysicsBackend::Stats Snapshot of entity/pair/contact counts.
     * @warning Returned data references the backend; invalidate after next tick.
     */
    [[nodiscard]] const IPhysicsBackend::Stats& stats() const noexcept { return stats_cache_; }

private:
    static constexpr std::string_view name_ = "physics";
    std::unique_ptr<IPhysicsBackend> backend_;
    IPhysicsBackend::Stats stats_cache_{};
};

}  // namespace evolution::sim
