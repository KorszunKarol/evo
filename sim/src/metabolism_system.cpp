#include "evolution/sim/metabolism_system.h"

#include "evolution/sim/population_monitor.h"

namespace evolution::sim {

MetabolismSystem::MetabolismSystem(bool destroy_on_zero) noexcept
    : destroy_on_zero_(destroy_on_zero) {}

void MetabolismSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const double dt = context.fixed_dt();

    recycle_bin_.clear();

    auto view = registry.view<MetabolismComponent>();
    view.each([&](const entt::entity entity, MetabolismComponent& metabolism) {
        if (dt > 0.0 && metabolism.basal_rate != 0.0) {
            metabolism.energy -= metabolism.basal_rate * dt;
        }

        if (metabolism.energy > metabolism.max_energy) {
            metabolism.energy = metabolism.max_energy;
        } else if (metabolism.energy < 0.0) {
            metabolism.energy = 0.0;
        }

        if (destroy_on_zero_ && metabolism.energy <= 0.0) {
            recycle_bin_.push_back(entity);
        }
    });

    if (!destroy_on_zero_) {
        return;
    }

    for (const auto entity : recycle_bin_) {
        if (registry.valid(entity)) {
            registry.destroy(entity);
            if (auto* counters = registry.ctx().find<PopulationEventCounters>()) {
                ++counters->deaths_total;
            }
        }
    }
}

}  // namespace evolution::sim

