#include "evolution/sim/stats_system.h"

#include <cmath>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/soil_volume.h"
#include "evolution/sim/population_monitor.h"

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

    const std::size_t alive_entities = registry.storage<entt::entity>().in_use();

    double energy_sum = 0.0;
    std::size_t metabolism_count = 0;
    auto metabolism_view = registry.view<MetabolismComponent>();
    metabolism_view.each([&](const MetabolismComponent& metabolism) {
        energy_sum += metabolism.energy;
        ++metabolism_count;
    });

    const double mean_energy = metabolism_count > 0
                                   ? energy_sum / static_cast<double>(metabolism_count)
                                   : 0.0;

    double plant_energy_sum = 0.0;
    std::size_t plant_count = 0;
    auto plant_view = registry.view<PlantComponent>();
    plant_view.each([&](const PlantComponent& plant) {
        if (!plant.alive) {
            return;
        }
        plant_energy_sum += plant.energy;
        ++plant_count;
    });
    const double mean_plant_energy = plant_count > 0
                                         ? plant_energy_sum / static_cast<double>(plant_count)
                                         : 0.0;

    std::size_t herbivore_count = 0;
    double herbivore_energy_sum = 0.0;
    auto herbivore_view = registry.view<MetabolismComponent, HerbivoreTag>();
    for (auto entity : herbivore_view) {
        const auto& metabolism = herbivore_view.get<MetabolismComponent>(entity);
        herbivore_energy_sum += metabolism.energy;
        ++herbivore_count;
    }
    const double mean_herbivore_energy = herbivore_count > 0
                                             ? herbivore_energy_sum / static_cast<double>(herbivore_count)
                                             : 0.0;

    double soil_mean = 0.0;
    if (auto* soil = registry.ctx().find<SoilGrid>()) {
        soil_mean = soil->mean_nutrient();
    } else if (auto* volume = registry.ctx().find<SoilVolume>()) {
        soil_mean = volume->mean_nitrogen();
    }

    double feeding_rate = 0.0;
    if (auto* stats = registry.ctx().find<FeedingStatistics>()) {
        feeding_rate = stats->energy_transferred_last_tick / context.fixed_dt();
    }

    double creature_density = 0.0;
    double herbivore_to_plant_ratio = 0.0;
    double carnivore_to_herbivore_ratio = 0.0;
    double population_stability_index = 0.0;
    if (const auto* monitor = registry.ctx().find<PopulationMonitor>()) {
        creature_density = monitor->latest.creature_density;
        herbivore_to_plant_ratio = monitor->latest.herbivore_to_plant_ratio;
        carnivore_to_herbivore_ratio = monitor->latest.carnivore_to_herbivore_ratio;
        population_stability_index = monitor->latest.population_stability_index;
    }

    double reproduction_rate = 0.0;
    double death_rate = 0.0;
    std::uint64_t rescue_count = 0;
    std::uint64_t cull_count = 0;
    if (const auto* counters = registry.ctx().find<PopulationEventCounters>()) {
        const double dt_window = std::max(1e-6, context.simulation_time() - prev_report_time_);
        const std::uint64_t births_delta = counters->births_total - prev_births_total_;
        const std::uint64_t deaths_delta = counters->deaths_total - prev_deaths_total_;
        reproduction_rate = static_cast<double>(births_delta) / dt_window;
        death_rate = static_cast<double>(deaths_delta) / dt_window;
        rescue_count = counters->rescues_total;
        cull_count = counters->culls_total;

        prev_births_total_ = counters->births_total;
        prev_deaths_total_ = counters->deaths_total;
        prev_rescues_total_ = counters->rescues_total;
        prev_culls_total_ = counters->culls_total;
    }
    prev_report_time_ = context.simulation_time();

    update_environment_stats(registry);

    spdlog::info("[t={:.2f}s] entities={} metabolism={} mean_energy={:.3f} plants={} mean_plant={:.3f} soil_mean={:.3f} herbivores={} herb_mean={:.3f} feed_rate={:.3f} density={:.6f} h:p={:.3f} c:h={:.3f} repro_rate={:.3f} death_rate={:.3f} stability={:.3f} rescues={} culls={}",
                 context.simulation_time(),
                 alive_entities,
                 metabolism_count,
                 mean_energy,
                 plant_count,
                 mean_plant_energy,
                 soil_mean,
                 herbivore_count,
                 mean_herbivore_energy,
                 feeding_rate,
                 creature_density,
                 herbivore_to_plant_ratio,
                 carnivore_to_herbivore_ratio,
                 reproduction_rate,
                 death_rate,
                 population_stability_index,
                 rescue_count,
                 cull_count);
}

}  // namespace evolution::sim
