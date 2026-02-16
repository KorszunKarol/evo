#include "evolution/sim/metabolism_system.h"

#include <sstream>

#include "evolution/sim/telemetry_system.h"

namespace evolution::sim {

MetabolismSystem::MetabolismSystem(bool destroy_on_zero) noexcept
    : destroy_on_zero_(destroy_on_zero) {}

void MetabolismSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const double dt = context.fixed_dt();
    auto* telemetry_ctx = registry.ctx().find<TelemetryContext>();
    TelemetrySystem* telemetry = telemetry_ctx != nullptr ? telemetry_ctx->system : nullptr;

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
            if (telemetry != nullptr) {
                const auto* genome = registry.try_get<GenomeHandleComponent>(entity);
                const genetics::GenomeId genome_id = genome != nullptr ? genome->id : 0;
                const bool force_capture = telemetry->should_capture(entity, 0, genome_id);

                std::ostringstream payload;
                payload << "{"
                        << "\"entity_id\":" << static_cast<std::uint32_t>(entity)
                        << ",\"genome_id\":" << genome_id
                        << ",\"cause\":\"STARVATION\""
                        << "}";

                TelemetryEvent event{
                    TelemetryEventType::ENTITY_DEATH,
                    context.simulation_time(),
                    payload.str()
                };
                telemetry->emit_event(event, force_capture);
            }
            recycle_bin_.push_back(entity);
        }
    });

    if (!destroy_on_zero_) {
        return;
    }

    for (const auto entity : recycle_bin_) {
        if (registry.valid(entity)) {
            registry.destroy(entity);
        }
    }
}

}  // namespace evolution::sim

