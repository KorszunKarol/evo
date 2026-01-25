#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "evolution/sim/simulation_context.h"

namespace evolution::sim {

///
/// @brief Interface for systems executed each simulation tick.
///
/// @details Systems encapsulate independent logic (physics, AI, metabolism) and operate over the shared ECS state.
/// @threadsafe Implementations must provide their own synchronization if accessing shared resources across threads.
///
class ISystem {
public:
    /// @brief Virtual destructor for safe polymorphic deletion.
    virtual ~ISystem() = default;

    ///
    /// @brief Performs a single tick of system logic.
    ///
    /// @param context SimulationContext& Provides access to world state and timing information.
    /// @complexity Implementation-defined; commonly O(entity_count).
    /// @notthreadsafe Systems typically mutate shared state and must provide their own synchronization if used concurrently.
    ///
    virtual void tick(SimulationContext& context) = 0;

    ///
    /// @brief Returns a human-readable system name used for logging and debugging.
    ///
    /// @return std::string_view Persistent name string.
    /// @threadsafe Implementations should provide thread-safe access if queried concurrently.
    ///
    virtual std::string_view name() const = 0;
};

///
/// @brief Manages ordered execution of registered systems.
///
/// @note Systems run sequentially; parallel execution will be introduced in later milestones.
///
class Scheduler {
public:
    ///
    /// @brief Adds a system to the execution list in registration order.
    ///
    /// @param system std::unique_ptr<ISystem> Ownership-transferring pointer to system instance.
    /// @warning Passing nullptr results in undefined behavior; callers must provide a valid system.
    /// @complexity O(1)
    /// @notthreadsafe Should be invoked during simulation setup on the main thread.
    ///
    void add_system(std::unique_ptr<ISystem> system);

    ///
    /// @brief Invokes all registered systems using the supplied context.
    ///
    /// @param context SimulationContext& Shared tick context propagated to each system.
    /// @complexity O(N) where N is the number of registered systems.
    /// @notthreadsafe Executes sequentially on the simulation thread.
    ///
    void tick_systems(SimulationContext& context);

    /**
     * @brief Returns system names in registration order.
     * @param None.
     * @return Ordered list of system names.
     * @note Intended for diagnostics and tests only.
     * @warning Do not use for scheduling logic in production.
     * @threadsafe Not thread-safe; call only during setup or when scheduler is idle.
     * @complexity O(N) where N is the number of registered systems.
     * @throws None.
     */
    [[nodiscard]] std::vector<std::string_view> system_names() const;

private:
    /// @brief Ordered list of systems invoked each tick.
    std::vector<std::unique_ptr<ISystem>> systems_;
};

}  // namespace evolution::sim
