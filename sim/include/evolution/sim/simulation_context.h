#pragma once

#include <entt/entt.hpp>

namespace evolution::sim {

///
/// @brief Bundles state passed to systems during a simulation tick.
///
/// @details Provides read/write access to the ECS registry alongside immutable timing metadata.
/// @threadsafe Not thread-safe; callers must ensure exclusive access when used across threads.
///
class SimulationContext {
public:
    ///
    /// @brief Constructs a context object bound to an ECS registry.
    ///
    /// @param registry entt::registry& Underlying registry storing entity data.
    /// @param fixed_dt double Fixed timestep duration in seconds.
    /// @param sim_time double Elapsed simulation time in seconds.
    ///
    SimulationContext(entt::registry& registry, double fixed_dt, double sim_time)
        : registry_(registry), fixed_dt_(fixed_dt), sim_time_(sim_time) {}

    ///
    /// @brief Retrieves the mutable registry reference.
    ///
    /// @return entt::registry& Reference allowing system modifications.
    ///
    [[nodiscard]] entt::registry& registry() { return registry_; }

    ///
    /// @brief Retrieves the immutable registry reference.
    ///
    /// @return const entt::registry& Reference for read-only operations.
    ///
    [[nodiscard]] const entt::registry& registry() const { return registry_; }

    ///
    /// @brief Reports the fixed timestep value.
    ///
    /// @return double Duration per tick in seconds.
    ///
    [[nodiscard]] double fixed_dt() const { return fixed_dt_; }

    ///
    /// @brief Reports the current simulation time.
    ///
    /// @return double Elapsed simulation time in seconds.
    ///
    [[nodiscard]] double simulation_time() const { return sim_time_; }

private:
    /// @brief Reference to the central ECS registry.
    entt::registry& registry_;
    /// @brief Fixed timestep interval.
    double fixed_dt_{0.0};
    /// @brief Elapsed simulation time at context creation.
    double sim_time_{0.0};
};

}  // namespace evolution::sim
