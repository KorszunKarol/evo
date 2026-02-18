#include "evolution/sim/population_monitor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <sstream>
#include <vector>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/phenotype_builder.h"
#include "evolution/sim/adaptive_control_system.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/telemetry_system.h"

namespace evolution::sim {

namespace {

[[nodiscard]] double safe_ratio(double numerator, double denominator) noexcept {
    if (std::abs(denominator) < 1e-9) {
        return 0.0;
    }
    return numerator / denominator;
}

[[nodiscard]] double derive_world_area(entt::registry& registry, const PopulationConfig& config) {
    if (config.world_area_override > 0.0) {
        return config.world_area_override;
    }
    if (const auto* terrain = registry.ctx().find<Terrain>()) {
        const double width = static_cast<double>(terrain->width()) * terrain->cell_size();
        const double height = static_cast<double>(terrain->height_cells()) * terrain->cell_size();
        return std::max(1.0, width * height);
    }
    return 1.0;
}

[[nodiscard]] Vec3 random_position_on_terrain(const Terrain& terrain, std::mt19937_64& rng) {
    std::uniform_real_distribution<double> dist_x(
        0.0, static_cast<double>(terrain.width() - 1) * terrain.cell_size());
    std::uniform_real_distribution<double> dist_z(
        0.0, static_cast<double>(terrain.height_cells() - 1) * terrain.cell_size());

    const double x = dist_x(rng);
    const double z = dist_z(rng);
    const double y = terrain.height(x, z);
    return Vec3{x, y, z};
}

}  // namespace

PopulationSystem::PopulationSystem(genetics::GenomeStorage& storage,
                                   const PopulationConfig& config,
                                   std::uint64_t genome_seed) noexcept
    : storage_(storage),
      config_(config),
      genome_seed_(genome_seed) {}

void PopulationSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    ++local_tick_;

    if (!registry.ctx().contains<PopulationMonitor>()) {
        registry.ctx().emplace<PopulationMonitor>(PopulationMonitor{.config = config_});
    }
    if (!registry.ctx().contains<PopulationEventCounters>()) {
        registry.ctx().emplace<PopulationEventCounters>();
    }

    auto& monitor = registry.ctx().get<PopulationMonitor>();
    monitor.config = config_;
    monitor.update_tick = local_tick_;

    update_snapshot(context, monitor);

    if (monitor.latest.below_min_viable) {
        ++monitor.below_min_streak_ticks;
    } else {
        monitor.below_min_streak_ticks = 0;
    }

    maybe_rescue(context, monitor);
    maybe_cull(context, monitor);
}

void PopulationSystem::update_snapshot(SimulationContext& context, PopulationMonitor& monitor) {
    auto& registry = context.registry();
    PopulationSnapshot snapshot{};

    auto creatures = registry.view<MetabolismComponent>();
    snapshot.creature_count = creatures.size();

    {
        auto herbivores = registry.view<MetabolismComponent, HerbivoreTag>();
        for ([[maybe_unused]] auto entity : herbivores) {
            ++snapshot.herbivore_count;
        }
    }
    {
        auto carnivores = registry.view<MetabolismComponent, CarnivoreTag>();
        for ([[maybe_unused]] auto entity : carnivores) {
            ++snapshot.carnivore_count;
        }
    }

    auto plants = registry.view<PlantComponent>();
    plants.each([&](const PlantComponent& plant) {
        if (plant.alive) {
            ++snapshot.plant_count;
        }
    });

    const double world_area = derive_world_area(registry, monitor.config);
    snapshot.creature_density = static_cast<double>(snapshot.creature_count) / std::max(1.0, world_area);
    snapshot.herbivore_to_plant_ratio = safe_ratio(static_cast<double>(snapshot.herbivore_count),
                                                   static_cast<double>(std::max<std::size_t>(1, snapshot.plant_count)));
    snapshot.carnivore_to_herbivore_ratio = safe_ratio(static_cast<double>(snapshot.carnivore_count),
                                                       static_cast<double>(std::max<std::size_t>(1, snapshot.herbivore_count)));

    snapshot.below_min_viable = snapshot.creature_count < monitor.config.min_viable_population;
    snapshot.above_soft_capacity = snapshot.creature_count > monitor.config.max_carry_capacity;
    snapshot.above_hard_cap = snapshot.creature_count > monitor.config.hard_cap;

    const double alpha = 2.0 / static_cast<double>(std::max<std::size_t>(2, monitor.config.monitor_ema_window) + 1);
    const double x = static_cast<double>(snapshot.creature_count);
    if (!monitor.ema_initialized) {
        monitor.ema_count = x;
        monitor.ema_count_sq = x * x;
        monitor.ema_initialized = true;
    } else {
        monitor.ema_count = alpha * x + (1.0 - alpha) * monitor.ema_count;
        monitor.ema_count_sq = alpha * (x * x) + (1.0 - alpha) * monitor.ema_count_sq;
    }
    snapshot.ema_creature_count = monitor.ema_count;
    snapshot.population_stability_index = std::max(0.0, monitor.ema_count_sq - monitor.ema_count * monitor.ema_count);

    monitor.latest = snapshot;
}

void PopulationSystem::maybe_rescue(SimulationContext& context, PopulationMonitor& monitor) {
    auto& registry = context.registry();
    double rescue_bias = 0.0;
    if (const auto* adaptive = registry.ctx().find<AdaptiveControlState>()) {
        rescue_bias = std::clamp(adaptive->rescue_bias, -1.0, 1.0);
    }

    const std::size_t effective_min_viable = static_cast<std::size_t>(std::llround(
        static_cast<double>(monitor.config.min_viable_population) * std::clamp(1.0 + 0.5 * rescue_bias, 0.5, 2.0)));
    const std::size_t effective_grace_ticks = static_cast<std::size_t>(std::llround(
        static_cast<double>(monitor.config.extinction_grace_ticks) / std::clamp(1.0 + 0.8 * rescue_bias, 0.4, 2.0)));
    const bool below_viable = monitor.latest.creature_count < effective_min_viable;
    if (!below_viable || monitor.below_min_streak_ticks < effective_grace_ticks) {
        return;
    }

    const auto* terrain = registry.ctx().find<Terrain>();
    if (terrain == nullptr) {
        return;
    }

    std::mt19937_64 rng(genome_seed_ ^ (monitor.rescue_events + 0x9E3779B97F4A7C15ULL) ^ local_tick_);

    std::size_t spawned = 0;
    const std::size_t effective_batch = static_cast<std::size_t>(std::llround(
        static_cast<double>(monitor.config.rescue_batch_size) * std::clamp(1.0 + rescue_bias, 0.5, 2.0)));
    for (; spawned < effective_batch; ++spawned) {
        const auto genome_seed = rng();
        const genetics::GenomeId genome_id = storage_.create_random(genome_seed);
        const entt::entity entity = registry.create();
        const auto result = genetics::PhenotypeBuilder::build(genome_id, registry, entity, storage_);
        if (!result.ok) {
            registry.destroy(entity);
            continue;
        }

        auto& transform = registry.get<TransformComponent>(entity);
        transform.position = random_position_on_terrain(*terrain, rng);

        auto& counters = registry.ctx().get<PopulationEventCounters>();
        ++counters.births_total;
    }

    monitor.below_min_streak_ticks = 0;
    ++monitor.rescue_events;
    auto& counters = registry.ctx().get<PopulationEventCounters>();
    ++counters.rescues_total;

    if (auto* telemetry_ctx = registry.ctx().find<TelemetryContext>();
        telemetry_ctx != nullptr && telemetry_ctx->system != nullptr) {
        std::ostringstream payload;
        payload << "{"
                << "\"before_count\":" << monitor.latest.creature_count
                << ",\"spawned\":" << spawned
                << ",\"after_count\":" << (monitor.latest.creature_count + spawned)
                << ",\"grace_ticks\":" << effective_grace_ticks
                << ",\"effective_min_viable\":" << effective_min_viable
                << "}";

        telemetry_ctx->system->emit_event(TelemetryEvent{.type = TelemetryEventType::POPULATION_RESCUE,
                                                         .sim_time = context.simulation_time(),
                                                         .payload = payload.str()},
                                          true);
    }
}

void PopulationSystem::maybe_cull(SimulationContext& context, PopulationMonitor& monitor) {
    auto& registry = context.registry();
    double cull_bias = 0.0;
    if (const auto* adaptive = registry.ctx().find<AdaptiveControlState>()) {
        cull_bias = std::clamp(adaptive->cull_bias, -1.0, 1.0);
    }
    const std::size_t effective_hard_cap = static_cast<std::size_t>(std::llround(
        static_cast<double>(monitor.config.hard_cap) * std::clamp(1.0 - 0.35 * cull_bias, 0.55, 1.5)));
    const std::size_t effective_target_cap = static_cast<std::size_t>(std::llround(
        static_cast<double>(monitor.config.target_cap) * std::clamp(1.0 - 0.35 * cull_bias, 0.55, 1.5)));

    if (monitor.latest.creature_count <= effective_hard_cap) {
        return;
    }

    struct Candidate {
        entt::entity entity{entt::null};
        double fitness{0.0};
        std::uint32_t id{0};
    };

    std::vector<Candidate> candidates;
    auto creature_view = registry.view<MetabolismComponent>();
    candidates.reserve(creature_view.size());
    for (auto entity : creature_view) {
        double fitness = std::numeric_limits<double>::infinity();
        if (const auto* fc = registry.try_get<FitnessComponent>(entity)) {
            fitness = fc->last_fitness;
        }
        candidates.push_back(Candidate{.entity = entity,
                                       .fitness = fitness,
                                       .id = static_cast<std::uint32_t>(entt::to_integral(entity))});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.fitness == b.fitness) {
            return a.id < b.id;
        }
        return a.fitness < b.fitness;
    });

    const std::size_t before_count = monitor.latest.creature_count;
    std::size_t current = before_count;
    std::size_t culled = 0;
    for (const auto& c : candidates) {
        if (current <= effective_target_cap) {
            break;
        }
        if (registry.valid(c.entity)) {
            registry.destroy(c.entity);
            --current;
            ++culled;
        }
    }

    if (culled == 0) {
        return;
    }

    ++monitor.cull_events;
    auto& counters = registry.ctx().get<PopulationEventCounters>();
    counters.deaths_total += culled;
    ++counters.culls_total;

    if (auto* telemetry_ctx = registry.ctx().find<TelemetryContext>();
        telemetry_ctx != nullptr && telemetry_ctx->system != nullptr) {
        std::ostringstream payload;
        payload << "{"
                << "\"before_count\":" << before_count
                << ",\"culled\":" << culled
                << ",\"after_count\":" << current
                << ",\"hard_cap\":" << effective_hard_cap
                << ",\"target_cap\":" << effective_target_cap
                << "}";

        telemetry_ctx->system->emit_event(TelemetryEvent{.type = TelemetryEventType::OVERPOPULATION_CULL,
                                                         .sim_time = context.simulation_time(),
                                                         .payload = payload.str()},
                                          true);
    }
}

}  // namespace evolution::sim
