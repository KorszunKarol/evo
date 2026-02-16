#include "evolution/sim/environment/plant_systems.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"

namespace evolution::sim {

PlantGrowthSystem::PlantGrowthSystem(double nutrient_to_energy) noexcept
    : nutrient_to_energy_(nutrient_to_energy) {}

void PlantGrowthSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    if (!registry.ctx().contains<SoilGrid>()) {
        return;
    }
    auto& soil = registry.ctx().get<SoilGrid>();
    const double dt = context.fixed_dt();

    auto view = registry.view<TransformComponent, PlantComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& plant = view.get<PlantComponent>(entity);

        if (!plant.alive) {
            plant.time_since_depleted += dt;
            continue;
        }

        plant.seed_timer += dt;

        const float soil_value = soil.sample(transform.position.x, transform.position.z);
        const double uptake_capacity = plant.growth_rate * dt * nutrient_to_energy_;
        const double uptake = std::min(static_cast<double>(soil_value), uptake_capacity);

        if (uptake > 0.0) {
            // Clamp to max energy while still reducing soil nutrients.
            const double new_energy = std::min(plant.max_energy, plant.energy + uptake);
            const double applied = new_energy - plant.energy;
            plant.energy = new_energy;

            const double cell_size = soil.cell_size();
            const int ix = std::clamp(static_cast<int>(transform.position.x / cell_size), 0, soil.width() - 1);
            const int iz = std::clamp(static_cast<int>(transform.position.z / cell_size), 0, soil.height() - 1);
            auto& cell = soil.at(ix, iz);
            cell = std::max(0.0F, cell - static_cast<float>(applied));
        }

        if (plant.energy <= 0.0) {
            plant.energy = 0.0;
            plant.alive = false;
            plant.time_since_depleted = 0.0;
        } else {
            plant.time_since_depleted = 0.0;
        }
    }
}

PlantSeedingSystem::PlantSeedingSystem(unsigned int seed) noexcept
    : rng_(seed) {}

void PlantSeedingSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const auto* terrain_ptr = registry.ctx().find<Terrain>();
    if (terrain_ptr == nullptr) {
        return;
    }
    auto* soil_ptr = registry.ctx().find<SoilGrid>();
    if (soil_ptr == nullptr) {
        return;
    }
    auto& soil = *soil_ptr;
    const auto* biome_map = registry.ctx().find<BiomeMap>();
    const auto* water_map = registry.ctx().find<WaterMap>();
    const auto* species_registry = registry.ctx().find<PlantSpeciesRegistry>();

    if (species_registry == nullptr) {
        spdlog::warn("PlantSeedingSystem missing PlantSpeciesRegistry");
        return;
    }
    const Terrain& terrain = *terrain_ptr;

    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * std::numbers::pi_v<double>);
    std::uniform_real_distribution<double> radius_dist(0.0, 1.0);
    std::uniform_real_distribution<double> energy_dist(3.0, 8.0);
    std::uniform_real_distribution<double> probability(0.0, 1.0);

    auto view = registry.view<TransformComponent, PlantComponent, PlantSeedParams>();

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& plant = view.get<PlantComponent>(entity);
        auto& params = view.get<PlantSeedParams>(entity);

        if (!plant.alive) {
            continue;
        }
        if (plant.energy < params.seed_min_energy) {
            continue;
        }
        if (plant.seed_timer + 1e-9 < plant.seed_interval) {
            continue;
        }

        plant.seed_timer = 0.0;

        const double angle = angle_dist(rng_);
        const double radius = std::sqrt(radius_dist(rng_)) * params.seed_radius;
        const double offset_x = std::cos(angle) * radius;
        const double offset_z = std::sin(angle) * radius;

        const double target_x = transform.position.x + offset_x;
        const double target_z = transform.position.z + offset_z;

        const double world_x = std::clamp(target_x, 0.0, static_cast<double>(terrain.width() - 1) * terrain.cell_size());
        const double world_z = std::clamp(target_z, 0.0, static_cast<double>(terrain.height_cells() - 1) * terrain.cell_size());
        const Vec3 normal = terrain.normal(world_x, world_z);
        if (normal.y < 0.1) {
            continue;
        }
        if (probability(rng_) > params.establish_probability) {
            continue;
        }

        const float soil_sample = soil.sample(world_x, world_z);
        if (soil_sample < 0.5F) {
            continue;
        }

        const BiomeId biome = biome_map != nullptr ? biome_map->sample(world_x, world_z) : BiomeId::Plains;
        double depth = 0.0;
        double shore_distance = std::numeric_limits<double>::infinity();
        WaterZone zone = WaterZone::Terrestrial;
        if (water_map != nullptr) {
            depth = water_map->depth(world_x, world_z);
            shore_distance = water_map->shore_distance(world_x, world_z);
            zone = classify_water_zone(*water_map, world_x, world_z);
        }

        const PlantSpecies& parent_species = species_registry->get(plant.species_id);
        if (!species_allows_location(parent_species, biome, zone, depth, shore_distance)) {
            continue;
        }

        TransformComponent child_transform{};
        child_transform.position = Vec3{world_x, terrain.height(world_x, world_z), world_z};
        const double child_energy = std::clamp(energy_dist(rng_), 3.0, plant.max_energy * 0.6);

        const entt::entity child = registry.create();
        registry.emplace<TransformComponent>(child, child_transform);

        PlantComponent child_component{};
        child_component.species_id = parent_species.id;
        child_component.max_energy = parent_species.max_energy;
        child_component.energy = std::clamp(child_energy, 2.0, parent_species.max_energy);
        child_component.radius = parent_species.radius;
        child_component.growth_rate = parent_species.growth_rate;
        child_component.seed_interval = parent_species.seed_interval;
        child_component.cleanup_delay = plant.cleanup_delay;
        registry.emplace<PlantComponent>(child, child_component);

        PlantSeedParams child_params{};
        child_params.seed_min_energy = std::max(1.0, parent_species.max_energy * 0.6);
        child_params.seed_cost = std::max(0.5, parent_species.max_energy * 0.25);
        child_params.seed_radius = parent_species.seed_radius;
        child_params.establish_probability = parent_species.establish_prob;
        registry.emplace<PlantSeedParams>(child, child_params);

        registry.emplace<NameComponent>(child, NameComponent{.value = "plant"});

        plant.energy = std::max(0.0, plant.energy - params.seed_cost);
    }

    update_environment_stats(registry);
}

void PlantCleanupSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const double dt = context.fixed_dt();

    auto view = registry.view<PlantComponent>();
    std::vector<entt::entity> to_destroy;
    for (auto entity : view) {
        auto& plant = view.get<PlantComponent>(entity);
        if (plant.alive) {
            continue;
        }
        plant.time_since_depleted += dt;
        if (plant.time_since_depleted >= plant.cleanup_delay) {
            to_destroy.push_back(entity);
        }
    }

    for (const entt::entity entity : to_destroy) {
        registry.destroy(entity);
    }
}

void PlantSpatialSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto* index = registry.ctx().find<PlantSpatialIndex>();
    if (index == nullptr) {
        return;
    }
    index->rebuild(registry);
}

}  // namespace evolution::sim
