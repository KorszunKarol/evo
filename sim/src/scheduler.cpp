#include "evolution/sim/scheduler.h"

#include <spdlog/spdlog.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace evolution::sim {

void Scheduler::add_system(std::unique_ptr<ISystem> system) {
    add_system(SystemStage::Ecology, std::move(system));
}

void Scheduler::add_system(SystemStage stage, std::unique_ptr<ISystem> system) {
    if (!system) {
        return;
    }
    const auto idx = static_cast<std::size_t>(stage);
    staged_systems_[idx].emplace_back(std::move(system));
}

void Scheduler::tick_systems(SimulationContext& context) {
    for (const auto& stage : staged_systems_) {
        for (const auto& system : stage) {
#ifdef TRACY_ENABLE
            ZoneScoped;
            ZoneText(system->name().data(), system->name().size());
#endif
            spdlog::trace("Ticking system: {}", system->name());
            system->tick(context);
        }
    }
}

}  // namespace evolution::sim
