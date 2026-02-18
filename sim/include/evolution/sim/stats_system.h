#pragma once

#include <cstdint>
#include <string_view>

#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Emits periodic population and energy statistics for diagnostics.
 *
 * @details Accumulates elapsed simulation time and, once the configured interval elapses,
 * computes population-wide metrics such as entity counts and mean metabolic energy, logging
 * the results through spdlog. The interval may be set to zero to force logging every tick.
 *
 * @note Designed for headless runs where quick health checks help validate large batches of simulations.
 * @notthreadsafe Must run on the simulation thread because it samples shared ECS state.
 */
class StatsSystem final : public ISystem {
public:
    /**
     * @brief Constructs the stats system with a reporting cadence.
     *
     * @param interval_seconds double Number of seconds between log statements (0.0 = every tick).
     * @throws None This constructor does not throw.
     * @complexity O(1)
     * @threadsafe @notthreadsafe Construction is thread-safe, though typical usage occurs on the simulation thread.
     * @example
     * StatsSystem stats{1.0}; // emit once per second
     * app.scheduler().add_system(std::make_unique<StatsSystem>(stats));
     */
    explicit StatsSystem(double interval_seconds = 1.0) noexcept;

    /**
     * @brief Samples registry state and logs aggregate statistics when the interval elapses.
     *
     * @param context SimulationContext& Provides registry access and timing metadata.
     * @throws None No exceptions are thrown.
     * @complexity O(N) where N is the number of entities with MetabolismComponent.
     * @threadsafe @notthreadsafe Invoke on the simulation thread; registry access is not thread-safe.
     * @warning Logging is skipped if no entities carry MetabolismComponent to avoid division by zero.
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Returns the system's diagnostic name.
     *
     * @return std::string_view Static literal identifier.
     * @throws None No exceptions are thrown.
     * @complexity O(1)
     * @threadsafe Thread-safe as long as the instance remains alive.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    /**
     * @brief Updates the reporting interval.
     *
     * @param interval_seconds double New cadence in seconds (values <= 0 force logging every tick).
     * @throws None No exceptions are thrown.
     * @complexity O(1)
     * @threadsafe @notthreadsafe Should be set during simulation setup.
     */
    void set_interval(double interval_seconds) noexcept { interval_ = interval_seconds; }

private:
    void emit_report(SimulationContext& context);

    static constexpr std::string_view name_ = "stats";
    double interval_{1.0};
    double accumulator_{0.0};
    std::uint64_t prev_births_total_{0};
    std::uint64_t prev_deaths_total_{0};
    std::uint64_t prev_rescues_total_{0};
    std::uint64_t prev_culls_total_{0};
    double prev_report_time_{0.0};
};

}  // namespace evolution::sim
