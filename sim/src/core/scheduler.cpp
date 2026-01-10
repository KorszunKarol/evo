#include "evolution/sim/scheduler.h"

#include <spdlog/spdlog.h>

namespace evolution::sim {

void Scheduler::add_system(std::unique_ptr<ISystem> system) {
    systems_.emplace_back(std::move(system));
}

void Scheduler::tick_systems(SimulationContext& context) {
    for (const auto& system : systems_) {
        spdlog::trace("Ticking system: {}", system->name());
        system->tick(context);
    }
}

}  // namespace evolution::sim
