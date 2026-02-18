#include "evolution/sim/simulation_app.h"

#include <spdlog/spdlog.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include "evolution/sim/simulation_context.h"

namespace evolution::sim {

SimulationApp::SimulationApp(SimulationConfig config)
    : fixed_dt_(config.fixed_dt) {
    spdlog::info("Simulation initialized with fixed dt = {}", fixed_dt_);
}

void SimulationApp::tick() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    begin_tick();
    SimulationContext context{registry_, fixed_dt_, simulation_time_};
    {
#ifdef TRACY_ENABLE
        ZoneScopedN("Scheduler");
#endif
        scheduler_.tick_systems(context);
    }
    end_tick();
#ifdef TRACY_ENABLE
    FrameMark;
#endif
}

void SimulationApp::run_for_steps(std::size_t steps) {
    for (std::size_t i = 0; i < steps; ++i) {
        tick();
    }
}

void SimulationApp::begin_tick() {
    spdlog::trace("Beginning tick {} at sim time {}", tick_count_, simulation_time_);
}

void SimulationApp::end_tick() {
    ++tick_count_;
    simulation_time_ += fixed_dt_;
    spdlog::trace("Finished tick {}. New sim time {}", tick_count_, simulation_time_);
}

}  // namespace evolution::sim
