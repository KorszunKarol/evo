#include "evolution/sim/scheduler.h"

#include <spdlog/spdlog.h>

namespace evolution::sim {

void Scheduler::add_system(std::unique_ptr<ISystem> system) {
    systems_.emplace_back(std::move(system));
}

void Scheduler::tick_systems(SimulationContext& context) {
    for (const auto& system : systems_) {
        SPDLOG_TRACE("Ticking system: {}", system->name());
        system->tick(context);
    }
}

std::vector<std::string_view> Scheduler::system_names() const {
    std::vector<std::string_view> names;
    names.reserve(systems_.size());
    for (const auto& system : systems_) {
        names.push_back(system->name());
    }
    return names;
}

}  // namespace evolution::sim
