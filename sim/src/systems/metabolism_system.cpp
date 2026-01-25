#include "evolution/sim/metabolism_system.h"

#include "evolution/sim/telemetry_system.h"

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
            // Determine death cause for telemetry
            if (auto* telem = registry.try_get<TelemetryComponent>(entity)) {
                if (telem->killed_by_predation) {
                    telem->death_cause = DeathCause::Predation;
                } else {
                    telem->death_cause = DeathCause::Starvation;
                }
            }

            // Log death event to telemetry system if attached
            if (telemetry_ != nullptr) {
                float x = 0.0f, z_coord = 0.0f;
                double lifetime = 0.0;
                if (auto* transform = registry.try_get<TransformComponent>(entity)) {
                    x = static_cast<float>(transform->position.x);
                    z_coord = static_cast<float>(transform->position.z);
                }
                if (auto* lifecycle = registry.try_get<LifecycleComponent>(entity)) {
                    lifetime = lifecycle->age;
                }
                auto* telem = registry.try_get<TelemetryComponent>(entity);
                std::uint8_t cause = telem ? static_cast<std::uint8_t>(telem->death_cause) : 0;
                telemetry_->log_death(context.simulation_time(), static_cast<std::uint64_t>(entity), cause, lifetime, x, z_coord);
            }

            // PHASE 2: Transform to Corpse instead of destroying
            // 1. Snapshot final state for the corpse
            double final_energy_budget = 0.0;
            if (auto* metabolism = registry.try_get<MetabolismComponent>(entity)) {
                // Biomass depends on size/mass and remaining energy (though it's <= 0 here)
                // Let's use max_energy as a proxy for biomass capacity
                final_energy_budget = metabolism->max_energy * 0.5; // Half of capacity remains as meat
            }

            // 2. Add CorpseComponent
            registry.emplace<CorpseComponent>(entity, CorpseComponent{
                .biomass = final_energy_budget,
                .max_biomass = final_energy_budget,
                .decay_rate = 0.05, // Slow decay (tunable)
                .toxicity = 0.0,
                .age = 0.0,
                .edible = true
            });

            // 3. Strip life-defining components to stop simulation of the entity
            registry.remove<BrainComponent>(entity);
            registry.remove<ActuationComponent>(entity);
            registry.remove<MetabolismComponent>(entity);
            registry.remove<VisionComponent>(entity);
            registry.remove<FeedingIntent>(entity);
            registry.remove<ReproductionComponent>(entity);
            
            // Note: We KEEP Transform, Kinematics (to allow falling/movement), 
            // Collider (to allow scavenger contact), and Telemetry (for later extraction).
            
            // Optionally change name for easier debugging
            if (auto* name = registry.try_get<NameComponent>(entity)) {
                name->value = "corpse_" + name->value;
            }
        }
    }
}

}  // namespace evolution::sim


