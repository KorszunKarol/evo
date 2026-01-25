/**
 * @file main.cpp
 * @brief Evolutionary run entry point with explicit generational loop.
 */

#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <entt/entt.hpp>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/phenotype_builder.h"
#include "evolution/sim/scenario.h"
#include "evolution/sim/components.h"
#include "evolution/sim/simulation_app.h"

// Systems
#include "evolution/sim/environment/feeding_system.h"
#include "evolution/sim/environment/plant_systems.h"
#include "evolution/sim/environment/soil_system.h"
#include "evolution/sim/fitness_update_system.h"
#include "evolution/sim/metabolism_system.h"
#include "evolution/sim/motor_system.h"
#include "evolution/sim/physics/simple_backend.h"
#include "evolution/sim/physics_system.h"
#include "evolution/sim/telemetry_system.h"
#include "evolution/sim/stats_system.h"
#include "evolution/sim/vision_system.h"
#include "evolution/sim/brain_inference_system.h"
#include "evolution/sim/system_slices.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/systems/trait_analysis_system.h"
#include "evolution/sim/systems/speciation_system.h"

using namespace evolution::sim;
using namespace evolution::genetics;
using namespace evolution::genome;

// Helper to spawn creatures
Vec3 random_position_on_terrain(const Terrain& terrain, std::mt19937_64& rng) {
    std::uniform_real_distribution<double> dist_x(0.0, (terrain.width() - 1) * terrain.cell_size());
    std::uniform_real_distribution<double> dist_z(0.0, (terrain.height_cells() - 1) * terrain.cell_size());
    const double x = dist_x(rng);
    const double z = dist_z(rng);
    const double y = terrain.height(x, z);
    return Vec3{x, y, z};
}

// Helper struct for spawning
struct SpawnRequest {
    GenomeId genome_id;
    std::uint64_t parent_id;
    std::uint32_t generation;
};

void spawn_generation(entt::registry& registry, 
                      GenomeStorage& storage, 
                      const std::vector<SpawnRequest>& requests, 
                      const Terrain& terrain,
                      std::uint64_t seed,
                      double current_time,
                      TelemetrySystem* telemetry = nullptr) {
    std::mt19937_64 rng(seed);
    
    for (const auto& req : requests) {
        const entt::entity entity = registry.create();
        auto result = PhenotypeBuilder::build(req.genome_id, registry, entity, storage);
        if (!result.ok) {
            registry.destroy(entity);
            continue;
        }
        
        auto& transform = registry.get<TransformComponent>(entity);
        transform.position = random_position_on_terrain(terrain, rng);
        
        // Store ID in name for retrieval for now
        std::string name = "creature_" + std::to_string(req.genome_id);
        registry.emplace_or_replace<NameComponent>(entity, NameComponent{.value = name});
        
        // Add Telemetry for tracking stats
        registry.emplace_or_replace<TelemetryComponent>(entity);
        
        if (auto* handle = registry.try_get<GenomeHandleComponent>(entity)) {
            handle->parent_id = req.parent_id;
            handle->generation = req.generation;
            
            if (telemetry) {
                telemetry->log_spawn(current_time, req.genome_id, req.parent_id, 
                                     static_cast<float>(transform.position.x), 
                                     static_cast<float>(transform.position.z));
            }
        }

    }
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Launching evolutionary simulation (50 Generations)");

    SimulationApp app;
    GenomeStorage genome_storage;
    SimulationScenario scenario{};
    
    // Config
    scenario.environment.terrain.width_cells = 64;
    scenario.environment.terrain.height_cells = 64;
    scenario.environment.terrain.cell_size = 2.0;
    scenario.environment.terrain.elevation_scale = 5.0;
    scenario.initial_population = 100;
    
    // Setup Environment manually
    initialize_environment(app.registry(), scenario.environment);
    seed_initial_plants(app.registry(), scenario.environment);
    
    // Physics
    const Terrain& terrain = app.registry().ctx().get<Terrain>();
    SimplePhysicsConfig physics_config{};
    physics_config.gravity = -9.81;
    physics_config.core.cell_size = 1.0;
    physics_config.enable_heightfield = true;
    auto backend = std::make_unique<SimplePhysicsBackend>(physics_config);
    auto* backend_ptr = backend.get();

    // Register Systems (Excluding ReproductionSystem)
    app.scheduler().add_system(std::make_unique<SoilSystem>());
    app.scheduler().add_system(std::make_unique<PlantGrowthSystem>());
    app.scheduler().add_system(std::make_unique<PlantSeedingSystem>(123));
    app.scheduler().add_system(std::make_unique<PlantSpatialSystem>());
    
    auto feeding_system = std::make_unique<FeedingSystem>();
    FeedingSystem* feeding_ptr = feeding_system.get();
    app.scheduler().add_system(std::move(feeding_system));
    
    app.scheduler().add_system(std::make_unique<PlantCleanupSystem>());
    const auto slice_handles =
        RegisterCreatureBehaviorSlice(app.scheduler(), genome_storage, *backend_ptr);
    MetabolismSystem* metabolism_ptr = slice_handles.metabolism;
    
    app.scheduler().add_system(std::make_unique<FitnessUpdateSystem>());
    
    auto telemetry_system = std::make_unique<TelemetrySystem>(1.0);
    TelemetrySystem* telemetry_ptr = telemetry_system.get();
    app.scheduler().add_system(std::move(telemetry_system));
    
    // Wire telemetry to systems
    feeding_ptr->set_telemetry(telemetry_ptr);
    metabolism_ptr->set_telemetry(telemetry_ptr);
    
    app.scheduler().add_system(std::make_unique<StatsSystem>(1.0));
    app.scheduler().add_system(std::make_unique<TraitAnalysisSystem>(10.0));
    app.scheduler().add_system(std::make_unique<SpeciationSystem>(genome_storage, 60.0));
    app.scheduler().add_system(std::make_unique<PhysicsSystem>(std::move(backend)));

    // Initial Population (Gen 0) - Random
    std::vector<SpawnRequest> current_gen_requests;
    std::mt19937_64 rng(scenario.genome_seed);
    for(size_t i=0; i<scenario.initial_population; ++i) {
        GenomeId id = genome_storage.create_random(rng());
        current_gen_requests.push_back({id, 0, 0}); // Parent 0, Gen 0
    }
    spawn_generation(app.registry(), genome_storage, current_gen_requests, terrain, scenario.genome_seed, 0.0, telemetry_ptr);

    // Evolutionary Loop
    const int generations = 5;
    const int steps_per_gen = 3000; // 50 seconds
    
    ReproConfig repro_config{}; // Defaults
    std::uint64_t evo_seed = scenario.reproduction_seed;

    for (int gen = 0; gen < generations; ++gen) {
        // Run
        spdlog::info("GEN_START: Generation {}", gen);
        app.run_for_steps(steps_per_gen);
        
        // Evaluate
        struct Candidate { GenomeId id; double fitness; DietType diet; };
        std::vector<Candidate> candidates;
        
        auto view = app.registry().view<NameComponent, TelemetryComponent, DietComponent>();
        int carnivore_count = 0;
        int herbivore_count = 0;
        double total_fitness = 0;

        for (auto entity : view) {
            const auto& name = view.get<NameComponent>(entity);
            const auto& telem = view.get<TelemetryComponent>(entity);
            const auto& diet = view.get<DietComponent>(entity);
            
            // Extract ID
            if (name.value.find("creature_") == 0) {
                 GenomeId id = std::stoull(name.value.substr(9));
                 // Fitness: Total energy + kills (predators)
                 // Note: Survivors only. Dead entities have already been removed.
                 double fit = 100.0 + telem.total_energy_gained * 0.1 + telem.kill_count * 50.0;
                 
                 candidates.push_back({id, fit, diet.type});
                 total_fitness += fit;
                 
                 if (diet.type == DietType::Carnivore) carnivore_count++;
                 else herbivore_count++;
            }
        }
        
        if (candidates.empty()) {
            spdlog::error("GEN_FAIL: Extinction event at Gen {}", gen);
            break;
        }

        // Stats
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b){
            return a.fitness > b.fitness;
        });
        
        spdlog::info("GEN_STATS: Gen={}, Herb={}, Carn={}, BestFit={:.2f}, AvgFit={:.2f}",
                     gen, herbivore_count, carnivore_count, candidates[0].fitness, total_fitness / candidates.size());

        // Selection (Simple Elitism + Random fill from top 50%)
        std::vector<SpawnRequest> next_gen_requests;
        
        // Elitism (Top 5)
        for(int i=0; i<std::min((int)5, (int)candidates.size()); ++i) {
            // Elites keep their generation and parent info? No, they are clones basically.
            // Let's increment gen for them or keep them as "survivors"?
            // Usually elitism implies the exact same individual survives.
            // But we act as if we respawn them. Let's say Gen+1, same ID.
            next_gen_requests.push_back({candidates[i].id, candidates[i].id, static_cast<std::uint32_t>(gen + 1)});
        }
        
        // Fill rest
        int cutoff = candidates.size() / 2;
        if (cutoff < 1) cutoff = 1;
        
        std::uniform_int_distribution<size_t> sel_dist(0, cutoff - 1);

        while(next_gen_requests.size() < scenario.initial_population) {
            // Select parents
            const auto& p1 = candidates[sel_dist(rng)];
            const auto& p2 = candidates[sel_dist(rng)]; // Can be same
            
            // Crossover
            const auto* g1 = genome_storage.get(p1.id);
            const auto* g2 = genome_storage.get(p2.id);
            
            GenomeT child_t;
            if (g1 && g2) {
                 child_t = crossover(*g1->UnPack(), *g2->UnPack(), repro_config, evo_seed++);
            } else if (g1) {
                child_t = *g1->UnPack();
            } else { // Should not happen
                continue; 
            }
            
            // Mutate
            child_t = mutate(std::move(child_t), repro_config, evo_seed++);
            
            // Store
            GenomeId child_id = genome_storage.insert(std::move(child_t));
            next_gen_requests.push_back({child_id, p1.id, static_cast<std::uint32_t>(gen + 1)});
        }
        
        current_gen_requests = next_gen_requests;
        
        // Reset Population (Destroy all creatures)
        auto creature_view = app.registry().view<NameComponent>(); // Assuming only creatures have names for now
        for (auto e : creature_view) {
             if (creature_view.get<NameComponent>(e).value.find("creature_") == 0) {
                 app.registry().destroy(e);
             }
        }
        
        // Respawn
        spawn_generation(app.registry(), genome_storage, current_gen_requests, terrain, evo_seed++, app.simulation_time(), telemetry_ptr);

        // Phase 4b: Attach Neural Probe to the first agent for Deep Observability
        const auto brain_view = app.registry().view<BrainComponent>();
        if (brain_view.begin() != brain_view.end()) {
            app.registry().emplace<BrainInspectComponent>(*brain_view.begin());
        }
        
        // Note: Plants accumulate/regrow naturally via systems.
    }

    spdlog::info("Evolution complete.");
    return 0;
}
