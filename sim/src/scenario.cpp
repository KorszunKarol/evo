#include "evolution/sim/scenario.h"

#include <random>
#include <string>

#include <spdlog/spdlog.h>

#include "evolution/genetics/phenotype_builder.h"
#include "evolution/sim/brain_inference_system.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/feeding_system.h"
#include "evolution/sim/environment/plant_systems.h"
#include "evolution/sim/environment/soil_system.h"
#include "evolution/sim/fitness_update_system.h"
#include "evolution/sim/metabolism_system.h"
#include "evolution/sim/motor_system.h"
#include "evolution/sim/physics/simple_backend.h"
#include "evolution/sim/physics_system.h"
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
        transform.position = random_position_on_terrain(*terrain, rng);

        const std::string name = "creature_" + std::to_string(genome_id);
        registry.emplace_or_replace<NameComponent>(entity, NameComponent{.value = name});
    }

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

    app.scheduler().add_system(std::make_unique<SoilSystem>());
    app.scheduler().add_system(std::make_unique<PlantGrowthSystem>());
    app.scheduler().add_system(std::make_unique<PlantSeedingSystem>(scenario.environment.plants.seed + 7));
    app.scheduler().add_system(std::make_unique<PlantSpatialSystem>());
    app.scheduler().add_system(std::make_unique<FeedingSystem>());
    app.scheduler().add_system(std::make_unique<PlantCleanupSystem>());
    app.scheduler().add_system(std::make_unique<BrainInferenceSystem>(storage));
    app.scheduler().add_system(std::make_unique<MotorSystem>());
    app.scheduler().add_system(std::make_unique<MetabolismSystem>());
    app.scheduler().add_system(std::make_unique<FitnessUpdateSystem>());
    app.scheduler().add_system(std::make_unique<ReproductionSystem>(storage,
                                                                    scenario.reproduction,
                                                                    scenario.reproduction_seed));
    if (scenario.enable_species_index) {
        app.scheduler().add_system(std::make_unique<SpeciesIndexSystem>(storage,
                                                                        scenario.reproduction));
    }
    app.scheduler().add_system(std::make_unique<PhysicsSystem>(std::move(backend)));
    app.scheduler().add_system(std::make_unique<StatsSystem>(1.0));

    if (scenario.enable_telemetry) {
        TelemetryTargeting targeting{};
        targeting.sampling_rate = scenario.telemetry_sampling_rate;
        RollupConfig rollup{};
        rollup.interval_seconds = scenario.telemetry_rollup_interval;
        rollup.buffer_size = scenario.telemetry_buffer_size;

        auto telemetry = std::make_unique<TelemetrySystem>(scenario.telemetry_output_dir,
                                                          scenario.telemetry_run_id,
                                                          targeting,
                                                          rollup);
        TelemetrySystem* telemetry_ptr = telemetry.get();
        app.scheduler().add_system(std::move(telemetry));
        app.registry().ctx().emplace<TelemetryContext>(TelemetryContext{.system = telemetry_ptr});
    }

    spdlog::info("Scenario ready: initial population {}", scenario.initial_population);
}

}  // namespace evolution::sim


