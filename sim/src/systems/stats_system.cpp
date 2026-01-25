#include "evolution/sim/stats_system.h"

#include <cmath>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"

namespace evolution::sim {

StatsSystem::StatsSystem(double interval_seconds) noexcept
    : interval_(interval_seconds) {}

void StatsSystem::tick(SimulationContext& context) {
    const double dt = context.fixed_dt();

    if (interval_ <= 0.0) {
        emit_report(context);
        return;
    }

    accumulator_ += dt;
    if (accumulator_ + 1e-9 < interval_) {
        return;
    }

    // Preserve fractional remainder to keep cadence stable.
    accumulator_ = std::fmod(accumulator_, interval_);
    emit_report(context);
}

void StatsSystem::emit_report(SimulationContext& context) {
    auto& registry = context.registry();

    double energy_sum = 0.0;
    std::size_t metabolism_count = 0;
    double biomass_consumers = 0.0;

    auto metabolism_view = registry.view<MetabolismComponent>();
    metabolism_view.each([&](const MetabolismComponent& metabolism) {
        energy_sum += metabolism.energy;
        ++metabolism_count;
        biomass_consumers += metabolism.energy;
    });

    double plant_energy_sum = 0.0;
    std::size_t plant_count = 0;
    double biomass_plants = 0.0;

    auto plant_view = registry.view<PlantComponent>();
    plant_view.each([&](const PlantComponent& plant) {
        if (!plant.alive) {
            return;
        }
        plant_energy_sum += plant.energy;
        ++plant_count;
        biomass_plants += plant.energy;
    });

    std::size_t herbivore_count = 0;
    auto herbivore_view = registry.view<MetabolismComponent, FeedingIntent>();
    herbivore_view.each([&](const MetabolismComponent&, const FeedingIntent&) {
        ++herbivore_count;
    });

    double biomass_soil = 0.0;
    double biomass_corpses = 0.0;
    std::size_t corpses = 0;

    auto corp_view = registry.view<CorpseComponent>();
    corp_view.each([&](const CorpseComponent& corpse) {
        biomass_corpses += corpse.biomass;
        ++corpses;
    });

    if (auto* soil = registry.ctx().find<SoilGrid>()) {
        biomass_soil = soil->mean_nutrient() * (soil->width() * soil->height());
    }

    double feeding_rate = 0.0;
    if (auto* stats = registry.ctx().find<FeedingStatistics>()) {
        feeding_rate = stats->energy_transferred_last_tick / context.fixed_dt();
    }

    update_environment_stats(registry);

    SPDLOG_INFO("[t={:.2f}s] Herb:{} Carn:{} Corpse:{} | BIO(k): Plant:{:.1f} Cons:{:.1f} Corpse:{:.1f} Soil:{:.1f} | Feed: {:.1f}",
                 context.simulation_time(),
                 herbivore_count,
                 metabolism_count - herbivore_count,
                 corpses,
                 biomass_plants / 1000.0,
                 biomass_consumers / 1000.0,
                 biomass_corpses / 1000.0,
                 biomass_soil / 1000.0,
                 feeding_rate);
}

}  // namespace evolution::sim

