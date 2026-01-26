#include "evolution/sim/scheduler.h"

#include <spdlog/spdlog.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace evolution::sim {

void Scheduler::add_system(std::unique_ptr<ISystem> system) {
    systems_.emplace_back(std::move(system));
}

void Scheduler::tick_systems(SimulationContext& context) {
    for (const auto& system : systems_) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        ZoneText(system->name().data(), system->name().size());
#endif
        spdlog::trace("Ticking system: {}", system->name());
        system->tick(context);
    }
}

}  // namespace evolution::sim
