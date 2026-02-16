#pragma once

#include <cstddef>

#include <entt/entt.hpp>

#include "evolution/sim/runtime_contracts.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

///
/// @brief Configuration options for initializing a simulation instance.
///
struct SimulationConfig {
    /// @brief Fixed timestep duration in seconds (default: 1/60).
    double fixed_dt{1.0 / 60.0};
};

///
/// @brief High-level façade managing simulation lifecycle and systems.
///
/// @details Owns the ECS registry and orchestrates tick progression for testing and headless execution.
/// @threadsafe Not thread-safe; caller must guarantee serialized tick invocation.
///
class SimulationApp : public IWorldTime {
public:
    ///
    /// @brief Constructs a simulation with the provided configuration.
    ///
    /// @param config SimulationConfig Optional configuration with timestep overrides.
    ///
    explicit SimulationApp(SimulationConfig config = {});

    ///
    /// @brief Advances the simulation by one fixed timestep.
    ///
    /// @note Invokes all registered systems sequentially.
    /// @complexity O(S) where S is the number of registered systems.
    /// @notthreadsafe Intended for main simulation thread execution.
    ///
    void tick();

    ///
    /// @brief Runs the simulation for a specified number of steps.
    ///
    /// @param steps std::size_t Positive number of ticks to execute.
    /// @complexity O(S * steps) where S is the number of registered systems.
    /// @notthreadsafe Uses sequential tick invocations on the main thread.
    ///
    void run_for_steps(std::size_t steps);

    ///
    /// @brief Exposes the mutable ECS registry.
    ///
    /// @return entt::registry& Reference to internal registry state.
    /// @notthreadsafe Mutations require single-threaded access.
    ///
    [[nodiscard]] entt::registry& registry() { return registry_; }

    ///
    /// @brief Provides access to the scheduler for system management.
    ///
    /// @return Scheduler& Reference to the scheduler instance.
    /// @notthreadsafe Scheduler manipulations must happen on the main thread.
    ///
    [[nodiscard]] Scheduler& scheduler() { return scheduler_; }

    ///
    /// @brief Fetches the accumulated simulation time.
    ///
    /// @return double Elapsed time in seconds.
    /// @notthreadsafe Value may change concurrently if ticks run on another thread.
    ///
    [[nodiscard]] double simulation_time() const noexcept override { return simulation_time_; }

    ///
    /// @brief Retrieves the configured fixed timestep duration.
    ///
    /// @return double Time per tick in seconds.
    /// @threadsafe Read-only constant value.
    ///
    [[nodiscard]] double fixed_dt() const noexcept override { return fixed_dt_; }

    [[nodiscard]] std::size_t tick_count() const noexcept override { return tick_count_; }

private:
    /// @brief Hook executed at the beginning of each tick for logging/metrics.
    void begin_tick();
    /// @brief Hook executed after systems complete; updates internal time counters.
    void end_tick();

    /// @brief Entity registry storing component data.
    entt::registry registry_{};
    /// @brief Scheduler managing system execution order.
    Scheduler scheduler_{};
    /// @brief Fixed timestep value in seconds.
    double fixed_dt_{0.0};
    /// @brief Accumulated simulation time in seconds.
    double simulation_time_{0.0};
    /// @brief Total number of ticks executed so far.
    std::size_t tick_count_{0};
};

}  // namespace evolution::sim
