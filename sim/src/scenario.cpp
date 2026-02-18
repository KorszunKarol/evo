#include "evolution/sim/scenario.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "evolution/genetics/phenotype_builder.h"
#include "evolution/sim/brain_inference_system.h"
#include "evolution/sim/adaptive_control_system.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/feeding_system.h"
#include "evolution/sim/environment/creature_spatial_index.h"
#include "evolution/sim/environment/plant_systems.h"
#include "evolution/sim/environment/soil_system.h"
#include "evolution/sim/fitness_update_system.h"
#include "evolution/sim/metabolism_system.h"
#include "evolution/sim/motor_system.h"
#include "evolution/sim/perception/vision_system.h"
#include "evolution/sim/physics/simple_backend.h"
#include "evolution/sim/physics_system.h"
#include "evolution/sim/population_monitor.h"
#include "evolution/sim/reproduction_system.h"
#include "evolution/sim/species_index_system.h"
#include "evolution/sim/stats_system.h"
#include "evolution/sim/telemetry_system.h"

namespace evolution::sim {

namespace {

[[nodiscard]] Vec3 random_position_on_terrain(const Terrain& terrain,
                                              std::mt19937_64& rng) {
    std::uniform_real_distribution<double> dist_x(
        0.0, static_cast<double>(terrain.width() - 1) * terrain.cell_size());
    std::uniform_real_distribution<double> dist_z(
        0.0, static_cast<double>(terrain.height_cells() - 1) * terrain.cell_size());

    const double x = dist_x(rng);
    const double z = dist_z(rng);
    const double y = terrain.height(x, z);
    return Vec3{x, y, z};
}

[[nodiscard]] Vec3 random_position_near_plants(entt::registry& registry,
                                               const Terrain& terrain,
                                               std::mt19937_64& rng,
                                               const std::vector<Vec3>& plant_positions) {
    if (plant_positions.empty()) {
        return random_position_on_terrain(terrain, rng);
    }

    std::uniform_int_distribution<std::size_t> pick_plant(0, plant_positions.size() - 1);
    std::uniform_real_distribution<double> jitter_angle(0.0, 2.0 * std::numbers::pi_v<double>);
    std::uniform_real_distribution<double> jitter_radius(0.0, 0.75);

    const Vec3 base = plant_positions[pick_plant(rng)];
    const double angle = jitter_angle(rng);
    const double radius = jitter_radius(rng);
    const double max_x = static_cast<double>(terrain.width() - 1) * terrain.cell_size();
    const double max_z = static_cast<double>(terrain.height_cells() - 1) * terrain.cell_size();
    const double x = std::clamp(base.x + std::cos(angle) * radius, 0.0, max_x);
    const double z = std::clamp(base.z + std::sin(angle) * radius, 0.0, max_z);
    const double y = terrain.height(x, z);
    return Vec3{x, y, z};
}

}  // namespace

void seed_initial_population(entt::registry& registry,
                             genetics::GenomeStorage& storage,
                             const SimulationScenario& scenario) {
    const auto* terrain = registry.ctx().find<Terrain>();
    if (terrain == nullptr) {
        spdlog::warn("Cannot spawn population: terrain service missing");
        return;
    }

    std::mt19937_64 rng(scenario.genome_seed);
    std::vector<Vec3> plant_positions;
    {
        auto plants = registry.view<TransformComponent, PlantComponent>();
        plant_positions.reserve(plants.size_hint());
        for (auto entity : plants) {
            const auto& plant = plants.get<PlantComponent>(entity);
            if (!plant.alive) {
                continue;
            }
            plant_positions.push_back(plants.get<TransformComponent>(entity).position);
        }
    }
    spdlog::info("Initial population seed context: sampled_plant_positions={}", plant_positions.size());
    double nearest_plant_dist_sum = 0.0;
    double nearest_plant_dist_max = 0.0;
    std::size_t nearest_plant_samples = 0;
    std::size_t adjusted_energy_count = 0;

    for (std::size_t i = 0; i < scenario.initial_population; ++i) {
        const genetics::GenomeId genome_id = storage.create_random(rng());
        const entt::entity entity = registry.create();
        const auto build_result = genetics::PhenotypeBuilder::build(genome_id, registry, entity, storage);
        if (!build_result.ok) {
            spdlog::warn("Failed to build phenotype: {}", build_result.msg);
            registry.destroy(entity);
            continue;
        }

        auto& transform = registry.get<TransformComponent>(entity);
        transform.position = random_position_near_plants(registry, *terrain, rng, plant_positions);
        if (auto* metabolism = registry.try_get<MetabolismComponent>(entity)) {
            if (const auto* diet = registry.try_get<DietComponent>(entity)) {
                if (diet->type == DietType::Herbivore) {
                    metabolism->energy = std::max(0.0, metabolism->max_energy * 0.65);
                } else {
                    metabolism->energy = std::max(0.0, metabolism->max_energy * 0.8);
                }
                ++adjusted_energy_count;
            }
        }
        if (!plant_positions.empty()) {
            double min_dist_sq = std::numeric_limits<double>::infinity();
            for (const Vec3& plant_pos : plant_positions) {
                const double dx = transform.position.x - plant_pos.x;
                const double dz = transform.position.z - plant_pos.z;
                const double dist_sq = dx * dx + dz * dz;
                min_dist_sq = std::min(min_dist_sq, dist_sq);
            }
            const double min_dist = std::sqrt(std::max(0.0, min_dist_sq));
            nearest_plant_dist_sum += min_dist;
            nearest_plant_dist_max = std::max(nearest_plant_dist_max, min_dist);
            ++nearest_plant_samples;
        }

        const std::string name = "creature_" + std::to_string(genome_id);
        registry.emplace_or_replace<NameComponent>(entity, NameComponent{.value = name});
    }

    if (nearest_plant_samples > 0) {
        spdlog::info("Initial population spawn diagnostics: avg_nearest_plant={:.3f}m max_nearest_plant={:.3f}m samples={}",
                     nearest_plant_dist_sum / static_cast<double>(nearest_plant_samples),
                     nearest_plant_dist_max,
                     nearest_plant_samples);
    }
    spdlog::info("Initial population energy adjustments applied to {} entities", adjusted_energy_count);

    spdlog::info("Seeded {} initial genomes", scenario.initial_population);
}

void setup_scenario(SimulationApp& app,
                    genetics::GenomeStorage& storage,
                    const SimulationScenario& scenario) {
    spdlog::info("Initializing environment");
    initialize_environment(app.registry(), scenario.environment);
    seed_initial_plants(app.registry(), scenario.environment);
    seed_initial_population(app.registry(), storage, scenario);

    const Terrain& terrain = app.registry().ctx().get<Terrain>();

    SimplePhysicsConfig physics_config{};
    physics_config.gravity = -9.81;
    physics_config.ground_height = terrain.min_y();
    physics_config.core.cell_size = 1.0;
    physics_config.core.solver_iterations = 8;
    physics_config.core.baumgarte = 0.25;
    physics_config.core.penetration_slop = 0.01;
    physics_config.enable_heightfield = true;

    auto backend = std::make_unique<SimplePhysicsBackend>(physics_config);

    app.scheduler().add_system(Scheduler::SystemStage::PrePhysics, std::make_unique<SoilSystem>());
    app.scheduler().add_system(Scheduler::SystemStage::PrePhysics, std::make_unique<PlantGrowthSystem>());
    PlantSeedingSystem::Tuning plant_seeding_tuning{};
    plant_seeding_tuning.seed = scenario.environment.plants.seed + 7;
    plant_seeding_tuning.update_interval_s = std::max(0.0, scenario.population.seeding_update_interval_s);
    app.scheduler().add_system(Scheduler::SystemStage::PrePhysics,
                               std::make_unique<PlantSeedingSystem>(plant_seeding_tuning));
    app.scheduler().add_system(Scheduler::SystemStage::PrePhysics, std::make_unique<PlantSpatialSystem>());
    app.scheduler().add_system(Scheduler::SystemStage::PrePhysics, std::make_unique<CreatureSpatialSystem>());
    app.scheduler().add_system(Scheduler::SystemStage::PrePhysics, std::make_unique<VisionSystem>());
    FeedingSystem::Tuning feeding_tuning{};
    feeding_tuning.enable_debug = scenario.enable_feeding_debug;
    feeding_tuning.debug_interval_s = scenario.feeding_debug_interval_s;
    feeding_tuning.predation_damage_scale = scenario.predation_damage_scale;
    feeding_tuning.predation_conversion_efficiency_scale = scenario.predation_conversion_efficiency_scale;
    feeding_tuning.pursuit_timeout_scale = scenario.pursuit_timeout_scale;
    feeding_tuning.attack_cooldown_scale = scenario.attack_cooldown_scale;
    app.scheduler().add_system(Scheduler::SystemStage::Ecology,
                               std::make_unique<FeedingSystem>(feeding_tuning));
    app.scheduler().add_system(Scheduler::SystemStage::Ecology, std::make_unique<PlantCleanupSystem>());
    app.scheduler().add_system(Scheduler::SystemStage::Ecology, std::make_unique<BrainInferenceSystem>(storage));
    app.scheduler().add_system(Scheduler::SystemStage::Ecology, std::make_unique<MotorSystem>());
    app.scheduler().add_system(Scheduler::SystemStage::Ecology, std::make_unique<MetabolismSystem>());
    app.scheduler().add_system(Scheduler::SystemStage::Ecology, std::make_unique<FitnessUpdateSystem>());
    auto reproduction_system =
        std::make_unique<ReproductionSystem>(storage, scenario.reproduction, scenario.reproduction_seed);
    reproduction_system->set_asexual_fallback(true);
    app.scheduler().add_system(Scheduler::SystemStage::Ecology, std::move(reproduction_system));
    if (scenario.enable_population_system) {
        app.scheduler().add_system(Scheduler::SystemStage::Ecology,
                                   std::make_unique<PopulationSystem>(storage,
                                                                      scenario.population,
                                                                      scenario.genome_seed));
    }
    if (scenario.enable_species_index) {
        app.scheduler().add_system(Scheduler::SystemStage::Ecology,
                                   std::make_unique<SpeciesIndexSystem>(storage,
                                                                        scenario.reproduction,
                                                                        10,
                                                                        3.0,
                                                                        scenario.species_index_interval_s,
                                                                        scenario.enable_species_info_logs));
    }
    app.scheduler().add_system(Scheduler::SystemStage::Physics, std::make_unique<PhysicsSystem>(std::move(backend)));
    app.scheduler().add_system(Scheduler::SystemStage::Metrics,
                               std::make_unique<StatsSystem>(scenario.stats_interval_s));
    app.scheduler().add_system(Scheduler::SystemStage::Metrics,
                               std::make_unique<AdaptiveControlSystem>(scenario.adaptive_control));

    if (scenario.enable_telemetry) {
        TelemetryTargeting targeting{};
        targeting.sampling_rate = scenario.telemetry_sampling_rate;
        RollupConfig rollup{};
        rollup.interval_seconds = scenario.telemetry_rollup_interval;
        rollup.buffer_size = scenario.telemetry_buffer_size;
        rollup.max_events_per_second = scenario.telemetry_max_events_per_second;
        rollup.max_events_per_type_per_second = scenario.telemetry_max_events_per_type_per_second;
        rollup.movement_capture_mode = scenario.telemetry_movement_capture_mode;

        auto telemetry = std::make_unique<TelemetrySystem>(scenario.telemetry_output_dir,
                                                          scenario.telemetry_run_id,
                                                          targeting,
                                                          rollup);
        TelemetrySystem* telemetry_ptr = telemetry.get();
        app.scheduler().add_system(Scheduler::SystemStage::Metrics, std::move(telemetry));
        app.registry().ctx().emplace<TelemetryContext>(TelemetryContext{.system = telemetry_ptr});
    }

    if (!app.registry().ctx().contains<PopulationEventCounters>()) {
        app.registry().ctx().emplace<PopulationEventCounters>();
    }
    if (!app.registry().ctx().contains<PopulationMonitor>()) {
        app.registry().ctx().emplace<PopulationMonitor>(PopulationMonitor{.config = scenario.population});
    }
    if (!app.registry().ctx().contains<AdaptiveControlState>()) {
        app.registry().ctx().emplace<AdaptiveControlState>(AdaptiveControlState{.config = scenario.adaptive_control});
    }

    spdlog::info("Scenario ready: initial population {}", scenario.initial_population);
}

}  // namespace evolution::sim
