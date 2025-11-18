#pragma once

#include <vector>
#include <functional>
#include <cstdint>
#include <string>

#include "evolution/sim/simulation_context.h"

namespace evolution::sim {

///
/// @brief Frequency at which a system should execute.
///
enum class SystemFrequency {
    Tick,    ///< Runs every simulation tick (highest frequency).
    Hourly,  ///< Runs once per simulation hour.
    Daily    ///< Runs once per simulation day.
};

///
/// @brief Orchestrates system execution across different timescales.
///
/// @details Manages buckets of systems corresponding to different update frequencies.
///          Ensures 'Hourly' and 'Daily' systems run at the appropriate intervals
///          based on the simulation time.
/// @threadsafe Not thread-safe. Must be ticked from the main simulation loop.
///
class MultiRateScheduler {
public:
    using SystemFunction = std::function<void(SimulationContext&)>;

    ///
    /// @brief Registers a system to run at the specified frequency.
    ///
    /// @param frequency SystemFrequency Time bucket for this system.
    /// @param system SystemFunction Callable system object.
    /// @param name std::string Debug name for the system.
    ///
    void add_system(SystemFrequency frequency, SystemFunction system, std::string name = "Unknown");

    ///
    /// @brief Executes systems based on the current tick and time accumulation.
    ///
    /// @param context SimulationContext Current simulation state and time.
    /// @note Checks if hourly/daily thresholds are crossed and executes respective systems.
    ///
    void tick(SimulationContext& context);

    ///
    /// @brief Sets the configuration for time scales.
    ///
    /// @param ticks_per_hour Number of ticks representing one simulation hour.
    /// @param hours_per_day Number of simulation hours in a day.
    ///
    void configure_time_scales(std::uint32_t ticks_per_hour, std::uint32_t hours_per_day);

private:
    struct SystemEntry {
        SystemFunction func;
        std::string name;
    };

    /// @brief Storage for systems in each frequency bucket.
    std::vector<SystemEntry> tick_systems_;
    std::vector<SystemEntry> hourly_systems_;
    std::vector<SystemEntry> daily_systems_;

    /// @brief Configured ticks per virtual hour.
    std::uint32_t ticks_per_hour_{3600}; // Default: assuming 60Hz = 60 simulation seconds per real second? No, 60 ticks = 1 sec? 
                                         // If 1 tick = 1/60s, then 1 hour = 3600s = 216000 ticks.
                                         // Let's default to something reasonable or let it be configured.
                                         // Plan says "Hourly (e.g. soil water)"

    /// @brief Configured hours per virtual day.
    std::uint32_t hours_per_day_{24};

    /// @brief Internal tick counter to track intervals.
    std::uint64_t total_ticks_{0};
};

}  // namespace evolution::sim

