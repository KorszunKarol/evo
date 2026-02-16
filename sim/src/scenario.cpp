#include "evolution/sim/scenario.h"

#include <filesystem>
#include <random>
#include <sstream>
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
#include "evolution/genetics/trait_extraction.h"

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

        if (auto* telemetry_ctx = registry.ctx().find<TelemetryContext>()) {
            TelemetrySystem* telemetry = telemetry_ctx->system;
            if (telemetry != nullptr) {
                const bool force_capture = telemetry->should_capture(entity, 0, genome_id);

                std::ostringstream spawn_payload;
                spawn_payload << "{"
                             << "\"entity_id\":" << static_cast<std::uint32_t>(entity)
                             << ",\"genome_id\":" << genome_id
                             << ",\"initial\":true"
                             << "}";

                TelemetryEvent spawn_event{
                    TelemetryEventType::ENTITY_SPAWN,
                    0.0,
                    spawn_payload.str()
                };
                telemetry->emit_event(spawn_event, force_capture);

                if (const auto* genome_ptr = storage.get(genome_id)) {
                    const auto traits = genetics::ExtractTraitVector(*genome_ptr);
                    std::ostringstream traits_payload;
                    traits_payload << "{"
                                   << "\"genome_id\":" << genome_id
                                   << ",\"traits\":["
                                   << traits[0] << "," << traits[1] << "," << traits[2] << "," << traits[3]
                                   << "," << traits[4] << "," << traits[5] << "," << traits[6] << "," << traits[7]
                                   << "]"
                                   << "}";
                    TelemetryEvent traits_event{
                        TelemetryEventType::GENOME_TRAITS,
                        0.0,
                        traits_payload.str()
                    };
                    telemetry->emit_event(traits_event, force_capture);
                }
            }
        }
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
        auto species_system = std::make_unique<SpeciesIndexSystem>(storage,
                                                                    scenario.reproduction);
        auto& ctx = app.registry().ctx();
        if (ctx.contains<SpeciesIndexContext>()) {
            ctx.erase<SpeciesIndexContext>();
        }
        ctx.emplace<SpeciesIndexContext>(SpeciesIndexContext{species_system.get()});
        app.scheduler().add_system(std::move(species_system));
    }
    if (scenario.enable_telemetry) {
        TelemetryTargeting targeting{};
        targeting.sampling_rate = scenario.telemetry_sampling_rate;

        RollupConfig rollup_config{};
        rollup_config.interval_seconds = scenario.telemetry_rollup_interval;
        rollup_config.buffer_size = scenario.telemetry_buffer_size;

        auto telemetry_system = std::make_unique<TelemetrySystem>(
            std::filesystem::path{scenario.telemetry_output_dir},
            scenario.telemetry_run_id,
            targeting,
            rollup_config);
        auto& ctx = app.registry().ctx();
        if (ctx.contains<TelemetryContext>()) {
            ctx.erase<TelemetryContext>();
        }
        ctx.emplace<TelemetryContext>(TelemetryContext{telemetry_system.get()});
        app.scheduler().add_system(std::move(telemetry_system));
    }
    app.scheduler().add_system(std::make_unique<PhysicsSystem>(std::move(backend)));
    app.scheduler().add_system(std::make_unique<StatsSystem>(1.0));

    spdlog::info("Scenario ready: initial population {}", scenario.initial_population);
}

}  // namespace evolution::sim
