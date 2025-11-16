#include "test_fixtures.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/phenotype_builder.h"
#include "evolution/sim/environment/environment_bootstrap.h"

using namespace evolution::genetics;

namespace evolution::sim::test {

void SimulationFixture::SetUp() {
    spdlog::set_level(spdlog::level::warn);  // Reduce noise in tests

    // Initialize environment
    auto config = create_test_env_config(global_seed_);
    initialize_environment(app_.registry(), config);
}

void SimulationFixture::TearDown() {
    // Cleanup handled by destructors
}

void SimulationFixture::run_steps(std::size_t steps) {
    for (std::size_t i = 0; i < steps; ++i) {
        app_.tick();
    }
}

SimulationFixture::Snapshot SimulationFixture::take_snapshot() const {
    Snapshot snap;
    auto& registry = const_cast<SimulationApp&>(app_).registry();

    snap.entity_count = registry.storage<entt::entity>().in_use();

    // Count plants
    auto plant_view = registry.view<PlantComponent>();
    plant_view.each([&](const PlantComponent& plant) {
        if (plant.alive) {
            ++snap.plant_count;
            snap.total_biomass += plant.energy;
            ++snap.species_counts[plant.species_id];
        }
    });

    // Count herbivores
    auto herbivore_view = registry.view<MetabolismComponent, HerbivoreTag>();
    herbivore_view.each([&](const MetabolismComponent& metab) {
        ++snap.herbivore_count;
        snap.total_biomass += metab.energy;
    });

    // Soil mean
    if (auto* soil = registry.ctx().find<SoilGrid>()) {
        snap.mean_soil = soil->mean_nutrient();
    }

    // Per-biome biomass
    if (auto* biome_map = registry.ctx().find<BiomeMap>()) {
        auto plant_view2 = registry.view<TransformComponent, PlantComponent>();
        plant_view2.each([&](const TransformComponent& transform, const PlantComponent& plant) {
            if (plant.alive) {
                const BiomeId biome = biome_map->sample(transform.position.x, transform.position.z);
                snap.biome_biomass[biome] += plant.energy;
            }
        });
    }

    // State hash
    snap.state_hash = hash_entity_state(const_cast<entt::registry&>(registry));

    return snap;
}

evolution::genetics::GenomeId SimulationFixture::create_test_genome(std::uint64_t seed) {
    return storage_.create_random(seed);
}

entt::entity SimulationFixture::spawn_herbivore(const Vec3& position,
                                                 evolution::genetics::GenomeId genome_id) {
    auto& registry = app_.registry();
    const entt::entity entity = registry.create();

    registry.emplace<TransformComponent>(entity, TransformComponent{.position = position});

    KinematicsComponent kinematics{};
    kinematics.inverse_mass = 1.0;
    kinematics.linear_damping = 0.2;
    registry.emplace<KinematicsComponent>(entity, kinematics);

    ColliderComponent collider{};
    collider.type = ShapeType::CapsuleY;
    collider.capsule.radius = 0.45;
    collider.capsule.half_height = 0.6;
    registry.emplace<ColliderComponent>(entity, collider);
    registry.emplace<RigidbodyComponent>(entity);

    MetabolismComponent metabolism{};
    metabolism.energy = 50.0;
    metabolism.max_energy = 100.0;
    metabolism.basal_rate = 1.0;
    registry.emplace<MetabolismComponent>(entity, metabolism);

    FeedingIntent intent{};
    intent.request_eat = true;
    intent.reach = 1.5;
    intent.rate = 8.0;
    registry.emplace<FeedingIntent>(entity, intent);
    registry.emplace<HerbivoreTag>(entity);

    registry.emplace<GenomeHandleComponent>(entity, GenomeHandleComponent{genome_id});

    // Build phenotype to add other components
    [[maybe_unused]] const auto build_result =
        genetics::PhenotypeBuilder::build(genome_id, registry, entity, storage_);

    // Add fitness component
    FitnessComponent fitness{};
    fitness.age_seconds = 0.0;
    fitness.energy_int_accum = 0.0;
    fitness.offspring_count = 0;
    fitness.last_fitness = 0.0;
    registry.emplace<FitnessComponent>(entity, fitness);

    return entity;
}

entt::entity SimulationFixture::spawn_plant(const Vec3& position,
                                            std::uint8_t species_id,
                                            double energy) {
    auto& registry = app_.registry();
    const entt::entity entity = registry.create();

    registry.emplace<TransformComponent>(entity, TransformComponent{.position = position});

    PlantComponent plant{};
    plant.species_id = species_id;
    plant.energy = energy;
    plant.max_energy = 20.0;
    plant.growth_rate = 2.0;
    plant.radius = 0.6;
    plant.alive = true;
    registry.emplace<PlantComponent>(entity, plant);

    PlantSeedParams params{};
    params.seed_min_energy = 12.0;
    params.seed_cost = 4.0;
    params.seed_radius = 6.0;
    params.establish_probability = 0.65;
    registry.emplace<PlantSeedParams>(entity, params);

    return entity;
}

EnvironmentConfig create_test_env_config(std::uint32_t seed) {
    EnvironmentConfig config{};

    config.terrain.width_cells = 128;
    config.terrain.height_cells = 128;
    config.terrain.cell_size = 2.0;
    config.terrain.elevation_scale = 18.0;
    config.terrain.octaves = 5;
    config.terrain.base_frequency = 0.004;
    config.terrain.seed = seed;

    config.soil.width_cells = 64;
    config.soil.height_cells = 64;
    config.soil.cell_size = 4.0;
    config.soil.max_nutrient = 12.0F;
    config.soil.diffusion_rate = 0.45F;
    config.soil.regeneration_rate = 0.08F;
    config.soil.baseline_nutrient = 5.0F;

    config.biome.width_cells = 128;
    config.biome.height_cells = 128;
    config.biome.cell_size = 2.0;
    config.biome.seed = seed;
    config.biome.biome_count = 3;
    config.biome.octaves = 4;
    config.biome.base_frequency = 0.003;

    config.water.width_cells = 128;
    config.water.height_cells = 128;
    config.water.cell_size = 2.0;
    config.water.seed = seed;
    config.water.water_level_percentile = 0.25;
    config.water.min_flow_accumulation = 500.0;

    config.plants.initial_count = 100;
    config.plants.min_initial_energy = 6.0;
    config.plants.max_initial_energy = 16.0;
    config.plants.seed = seed + 1;

    config.plant_spatial_cell_size = 4.0;

    return config;
}

std::string hash_entity_state(entt::registry& registry) {
    std::ostringstream oss;

    // Hash entity count
    oss << registry.storage<entt::entity>().in_use() << ";";

    // Hash plant states
    auto plant_view = registry.view<TransformComponent, PlantComponent>();
    std::vector<std::pair<entt::entity, double>> plants;
    for (auto entity : plant_view) {
        const auto& plant = plant_view.get<PlantComponent>(entity);
        if (plant.alive) {
            const auto& transform = plant_view.get<TransformComponent>(entity);
            plants.emplace_back(entity, plant.energy + transform.position.x * 0.1 +
                                            transform.position.z * 0.1);
        }
    }
    std::sort(plants.begin(), plants.end());
    for (const auto& [entity, val] : plants) {
        oss << static_cast<std::uint32_t>(entity) << ":" << val << ";";
    }

    // Hash herbivore states
    auto herbivore_view = registry.view<TransformComponent, MetabolismComponent, GenomeHandleComponent>();
    std::vector<std::pair<entt::entity, double>> herbivores;
    for (auto entity : herbivore_view) {
        const auto& metab = herbivore_view.get<MetabolismComponent>(entity);
        const auto& handle = herbivore_view.get<GenomeHandleComponent>(entity);
        const auto& transform = herbivore_view.get<TransformComponent>(entity);
        herbivores.emplace_back(entity,
                                metab.energy + static_cast<double>(handle.id) * 0.01 +
                                    transform.position.x * 0.1);
    }
    std::sort(herbivores.begin(), herbivores.end());
    for (const auto& [entity, val] : herbivores) {
        oss << static_cast<std::uint32_t>(entity) << ":" << val << ";";
    }

    return oss.str();
}

}  // namespace evolution::sim::test

