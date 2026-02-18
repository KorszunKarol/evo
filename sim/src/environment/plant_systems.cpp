#include "evolution/sim/environment/plant_systems.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/soil_volume.h"
#include "evolution/sim/adaptive_control_system.h"
#include "evolution/sim/population_monitor.h"

namespace evolution::sim {

namespace {

[[nodiscard]] double safe_ratio(double numerator, double denominator) noexcept {
    if (std::abs(denominator) < 1e-9) {
        return 0.0;
    }
    return numerator / denominator;
}

[[nodiscard]] double smoothstep(double edge0, double edge1, double x) noexcept {
    if (edge1 <= edge0) {
        return x >= edge1 ? 1.0 : 0.0;
    }
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

}  // namespace

PlantGrowthSystem::PlantGrowthSystem(double nutrient_to_energy) noexcept
    : nutrient_to_energy_(nutrient_to_energy) {}

void PlantGrowthSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    
    SoilVolume* volume = nullptr;
    if (registry.ctx().contains<SoilVolume>()) {
        volume = &registry.ctx().get<SoilVolume>();
    }

    SoilGrid* soil_grid = nullptr;
    if (registry.ctx().contains<SoilGrid>()) {
        soil_grid = &registry.ctx().get<SoilGrid>();
    }

    if (volume == nullptr && soil_grid == nullptr) {
        return;
    }

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

        double soil_value = 0.0;
        if (volume) {
            // Use nitrogen as primary nutrient
            soil_value = static_cast<double>(volume->sample(transform.position).nitrogen);
        } else {
            soil_value = static_cast<double>(soil_grid->sample(transform.position.x, transform.position.z));
        }

        const double uptake_capacity = plant.growth_rate * dt * nutrient_to_energy_;
        const double uptake = std::min(soil_value, uptake_capacity);

        if (uptake > 0.0) {
            // Clamp to max energy while still reducing soil nutrients.
            const double new_energy = std::min(plant.max_energy, plant.energy + uptake);
            const double applied = new_energy - plant.energy;
            plant.energy = new_energy;

            if (volume) {
                // Discrete update
                int ix = static_cast<int>(transform.position.x / volume->voxel_size());
                int iy = static_cast<int>(transform.position.y / volume->voxel_size());
                int iz = static_cast<int>(transform.position.z / volume->voxel_size());
                
                ix = std::clamp(ix, 0, volume->width() - 1);
                iy = std::clamp(iy, 0, volume->height() - 1);
                iz = std::clamp(iz, 0, volume->depth() - 1);
                
                auto& voxel = volume->at(ix, iy, iz);
                const double current_n = static_cast<double>(voxel.nitrogen);
                voxel.nitrogen = static_cast<float>(std::max(0.0, current_n - applied));
            } else {
                const double cell_size = soil_grid->cell_size();
                const int ix = std::clamp(static_cast<int>(transform.position.x / cell_size), 0, soil_grid->width() - 1);
                const int iz = std::clamp(static_cast<int>(transform.position.z / cell_size), 0, soil_grid->height() - 1);
                auto& cell = soil_grid->at(ix, iz);
                cell = std::max(0.0F, cell - static_cast<float>(applied));
            }
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

PlantSeedingSystem::PlantSeedingSystem() noexcept
    : PlantSeedingSystem(Tuning{}) {}

PlantSeedingSystem::PlantSeedingSystem(Tuning tuning) noexcept
    : tuning_(tuning), rng_(tuning.seed) {}

PlantSeedingSystem::PlantSeedingSystem(unsigned int seed) noexcept
    : PlantSeedingSystem(Tuning{.seed = seed}) {}

void PlantSeedingSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const auto* terrain_ptr = registry.ctx().find<Terrain>();
    if (terrain_ptr == nullptr) {
        return;
    }
    
    SoilVolume* volume = nullptr;
    if (registry.ctx().contains<SoilVolume>()) {
        volume = &registry.ctx().get<SoilVolume>();
    }

    SoilGrid* soil_grid = nullptr;
    if (registry.ctx().contains<SoilGrid>()) {
        soil_grid = &registry.ctx().get<SoilGrid>();
    }

    if (volume == nullptr && soil_grid == nullptr) {
        return;
    }

    PopulationConfig population_config{};
    PopulationSnapshot population_snapshot{};
    if (const auto* monitor = registry.ctx().find<PopulationMonitor>()) {
        population_config = monitor->config;
        population_snapshot = monitor->latest;
    }

    const auto compute_soil_fraction = [&]() {
        if (soil_grid != nullptr) {
            const double max_nutrient = std::max(1.0, static_cast<double>(soil_grid->config().max_nutrient));
            return std::clamp(soil_grid->mean_nutrient() / max_nutrient, 0.0, 1.0);
        }
        if (volume != nullptr) {
            // SoilVolume regeneration currently tends toward biome baselines around ~3-6 nitrogen.
            return std::clamp(volume->mean_nitrogen() / 6.0, 0.0, 1.0);
        }
        return 1.0;
    };
    const double soil_fraction = compute_soil_fraction();
    const double depletion_threshold =
        std::clamp(population_config.soil_depletion_threshold, 0.01, 1.0);
    const double min_multiplier =
        std::clamp(population_config.min_plant_seeding_multiplier, 0.0, 1.0);
    const double max_multiplier =
        std::clamp(population_config.max_plant_seeding_multiplier, min_multiplier, 1.0);

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

    const auto active_plants = registry.view<PlantComponent>().size();
    const auto herbivore_count = population_snapshot.herbivore_count > 0
                                     ? population_snapshot.herbivore_count
                                     : registry.view<MetabolismComponent, HerbivoreTag>().size_hint();

    pressure_accumulator_ += context.fixed_dt();
    const double pressure_update_interval =
        std::max(0.0, std::min(population_config.seeding_update_interval_s, tuning_.update_interval_s));
    if (pressure_update_interval <= 0.0 || pressure_accumulator_ + 1e-9 >= pressure_update_interval) {
        if (pressure_update_interval > 0.0) {
            pressure_accumulator_ = std::fmod(pressure_accumulator_, pressure_update_interval);
        } else {
            pressure_accumulator_ = 0.0;
        }

        const double plant_pressure =
            std::clamp(population_config.plant_pressure_gain, 0.0, 4.0) *
            smoothstep(static_cast<double>(population_config.plant_soft_capacity),
                       static_cast<double>(population_config.plant_high_pressure_capacity),
                       static_cast<double>(active_plants));
        constexpr double kTargetHerbivoreToPlantRatio = 0.02;
        const double herbivore_to_plant = safe_ratio(static_cast<double>(herbivore_count),
                                                     static_cast<double>(std::max<std::size_t>(1, active_plants)));
        const double grazing_relief =
            std::clamp(population_config.herbivore_pressure_gain, 0.0, 4.0) *
            std::clamp(herbivore_to_plant / kTargetHerbivoreToPlantRatio, 0.0, 1.0);
        const double density_feedback =
            std::clamp(population_config.density_feedback_gain, 0.0, 4.0) *
            std::clamp((kTargetHerbivoreToPlantRatio - herbivore_to_plant) /
                           kTargetHerbivoreToPlantRatio,
                       0.0,
                       1.0);
        const double soil_term =
            std::clamp(soil_fraction / std::max(1e-6, depletion_threshold), 0.0, 1.0);
        pressure_multiplier_ = std::clamp((1.0 - plant_pressure - density_feedback + grazing_relief) *
                                              soil_term,
                                          min_multiplier,
                                          max_multiplier);
    }
    double seeding_multiplier = pressure_multiplier_;
    if (const auto* adaptive = registry.ctx().find<AdaptiveControlState>()) {
        if (adaptive->plant_seeding_multiplier_override > 0.0) {
            seeding_multiplier =
                std::clamp(adaptive->plant_seeding_multiplier_override, min_multiplier, max_multiplier);
        }
    }

    const auto* spatial_index = registry.ctx().find<PlantSpatialIndex>();

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
        
        // Probabilistic early exit to reduce checking cost at high counts
        if (active_plants > 10000 && probability(rng_) > 0.1) {
             continue; // Throttle seeding as we approach cap
        }

        const double angle = angle_dist(rng_);
        const double radius = std::sqrt(radius_dist(rng_)) * params.seed_radius;
        const double offset_x = std::cos(angle) * radius;
        const double offset_z = std::sin(angle) * radius;

        const double target_x = transform.position.x + offset_x;
        const double target_z = transform.position.z + offset_z;
        
        // ... (Bounds checks remain the same) ...
        const double world_x = std::clamp(target_x, 0.0, static_cast<double>(terrain.width() - 1) * terrain.cell_size());
        const double world_z = std::clamp(target_z, 0.0, static_cast<double>(terrain.height_cells() - 1) * terrain.cell_size());
        
        // Density Check using Spatial Index
        if (spatial_index != nullptr) {
            bool too_crowded = false;
            // Increase spacing requirements as pressure rises to discourage local runaway patches.
            const double pressure = 1.0 - seeding_multiplier;
            const double check_radius = plant.radius * (0.8 + pressure * 0.7);
            spatial_index->for_each_in_radius(registry, Vec3{world_x, 0.0, world_z}, check_radius, 
                [&too_crowded](entt::entity, double) {
                    too_crowded = true;
                });
            if (too_crowded) continue;
        }

        const Vec3 normal = terrain.normal(world_x, world_z);
        if (normal.y < 0.45) {
            continue;
        }
        const double effective_establish_probability =
            std::clamp(params.establish_probability * seeding_multiplier, 0.0, 1.0);
        if (probability(rng_) > effective_establish_probability) {
            continue;
        }

        float soil_sample = 0.0F;
        if (volume) {
            // Assume surface sample or slightly below
            soil_sample = volume->sample(Vec3{world_x, transform.position.y, world_z}).nitrogen;
        } else {
            soil_sample = soil_grid->sample(world_x, world_z);
        }

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
