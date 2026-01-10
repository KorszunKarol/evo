#include "evolution/sim/metabolism_system.h"

namespace evolution::sim {

MetabolismSystem::MetabolismSystem(bool destroy_on_zero) noexcept
    : destroy_on_zero_(destroy_on_zero) {}

void MetabolismSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const double dt = context.fixed_dt();

    recycle_bin_.clear();

    auto view = registry.view<MetabolismComponent>();
    view.each([&](const entt::entity entity, MetabolismComponent& metabolism) {
        const double energy_loss = metabolism.basal_rate * dt;
        if (dt > 0.0 && metabolism.basal_rate != 0.0) {
            metabolism.energy -= energy_loss;
            
            // Telemetry: track metabolism energy loss
            if (auto* telem = registry.try_get<TelemetryComponent>(entity)) {
                telem->total_energy_lost_metabolism += energy_loss;
            }
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
            // Determine death cause before destruction
            if (auto* telem = registry.try_get<TelemetryComponent>(entity)) {
                if (telem->killed_by_predation) {
                    telem->death_cause = DeathCause::Predation;
                } else {
                    telem->death_cause = DeathCause::Starvation;
                }
            }
            registry.destroy(entity);
        }
    }
}

}  // namespace evolution::sim


