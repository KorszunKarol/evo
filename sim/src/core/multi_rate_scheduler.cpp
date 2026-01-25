#include "evolution/sim/multi_rate_scheduler.h"

#include <spdlog/spdlog.h>

namespace evolution::sim {

void MultiRateScheduler::add_system(SystemFrequency frequency, SystemFunction system, std::string name) {
    SystemEntry entry{std::move(system), std::move(name)};
    switch (frequency) {
        case SystemFrequency::Tick:
            tick_systems_.push_back(std::move(entry));
            break;
        case SystemFrequency::Hourly:
            hourly_systems_.push_back(std::move(entry));
            break;
        case SystemFrequency::Daily:
            daily_systems_.push_back(std::move(entry));
            break;
    }
}

void MultiRateScheduler::configure_time_scales(std::uint32_t ticks_per_hour, std::uint32_t hours_per_day) {
    ticks_per_hour_ = ticks_per_hour;
    hours_per_day_ = hours_per_day;
    spdlog::info("MultiRateScheduler configured: {} ticks/hour, {} hours/day", ticks_per_hour_, hours_per_day_);
}

void MultiRateScheduler::tick(SimulationContext& context) {
    // 1. Run Tick-rate systems (High Frequency)
    for (const auto& sys : tick_systems_) {
        sys.func(context);
    }

    // 2. Check Hourly
    // We use ++total_ticks_ at the end, or checking current?
    // Let's increment at the end to represent "finishing" a tick. 
    // So on tick 0, we are at time 0. Should we run hourly at start?
    // Usually initialization runs separately. Let's assume run on multiples.
    // If ticks_per_hour is 100, we run at 100, 200, 300.
    
    const std::uint64_t current_tick_count = total_ticks_ + 1; // Anticipate the end of this tick

    if (current_tick_count % ticks_per_hour_ == 0) {
        SPDLOG_TRACE("Running {} hourly systems at tick {}", hourly_systems_.size(), current_tick_count);
        for (const auto& sys : hourly_systems_) {
            sys.func(context);
        }

        // 3. Check Daily (only if hourly also triggered, assuming day is multiple of hours)
        std::uint64_t ticks_per_day = static_cast<std::uint64_t>(ticks_per_hour_) * hours_per_day_;
        if (current_tick_count % ticks_per_day == 0) {
            SPDLOG_DEBUG("Running {} daily systems at tick {}", daily_systems_.size(), current_tick_count);
            for (const auto& sys : daily_systems_) {
                sys.func(context);
            }
        }
    }

    total_ticks_++;
}

}  // namespace evolution::sim

