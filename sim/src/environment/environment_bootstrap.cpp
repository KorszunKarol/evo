#include "evolution/sim/environment/environment_bootstrap.h"

#include <algorithm>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/soil_volume.h"

namespace evolution::sim {

namespace {

[[nodiscard]] bool should_place_on_normal(const Vec3& normal) noexcept {
    constexpr double min_dot = 0.45;  // Reject slopes steeper than ~63 degrees.
    return normal.y >= min_dot;
}

[[nodiscard]] const PlantSpecies& select_species_for_location(const PlantSpeciesRegistry& registry,
                                                             BiomeId biome,
                                                             WaterZone zone,
                                                             double depth,
                                                             double shore_distance,
                                                             std::mt19937& rng) {
    std::vector<std::uint8_t> candidates;
    candidates.reserve(registry.count());
    for (std::uint8_t id = 0; id < registry.count(); ++id) {
        const auto& species = registry.get(id);
        if (species_allows_location(species, biome, zone, depth, shore_distance)) {
            candidates.push_back(id);
        }
    }

    if (candidates.empty()) {
        return registry.get(0);
    }

    std::uniform_int_distribution<std::size_t> choice(0, candidates.size() - 1);
    return registry.get(candidates[choice(rng)]);
}

}  // namespace

void initialize_environment(entt::registry& registry, const EnvironmentConfig& config) {
    auto& ctx = registry.ctx();

    // Initialize terrain first (required for biome and water maps)
    Terrain* terrain_ptr = nullptr;
    if (!ctx.contains<Terrain>()) {
        terrain_ptr = &ctx.emplace<Terrain>(config.terrain);
    } else {
        ctx.erase<Terrain>();
        terrain_ptr = &ctx.emplace<Terrain>(config.terrain);
    }
    const Terrain& terrain = *terrain_ptr;

    // Initialize biome map (depends on terrain)
    BiomeConfig biome_config = config.biome;
    biome_config.width_cells = terrain.width();
    biome_config.height_cells = terrain.height_cells();
    biome_config.cell_size = terrain.cell_size();
    biome_config.seed = config.terrain.seed;  // Use terrain seed for consistency

    if (!ctx.contains<BiomeMap>()) {
        ctx.emplace<BiomeMap>(biome_config, terrain);
    } else {
        ctx.erase<BiomeMap>();
        ctx.emplace<BiomeMap>(biome_config, terrain);
    }

    // Initialize water map (depends on terrain)
    WaterConfig water_config = config.water;
    water_config.width_cells = terrain.width();
    water_config.height_cells = terrain.height_cells();
    water_config.cell_size = terrain.cell_size();
    water_config.seed = config.terrain.seed;  // Use terrain seed for consistency

    if (!ctx.contains<WaterMap>()) {
        ctx.emplace<WaterMap>(water_config, terrain);
    } else {
        ctx.erase<WaterMap>();
        ctx.emplace<WaterMap>(water_config, terrain);
    }

    // Initialize soil grid (2D legacy)
    if (!ctx.contains<SoilGrid>()) {
        ctx.emplace<SoilGrid>(config.soil);
    } else {
        ctx.erase<SoilGrid>();
        ctx.emplace<SoilGrid>(config.soil);
    }

    // Initialize soil volume (3D)
    SoilVolumeConfig vol_config;
    vol_config.width = config.soil.width_cells;
    vol_config.depth = config.soil.height_cells; // In config 'height_cells' is Z
    vol_config.height = 16; // Default vertical depth for now
    vol_config.voxel_size = config.soil.cell_size;
    vol_config.diffusion_rate = math::Fixed64(config.soil.diffusion_rate);
    
    if (!ctx.contains<SoilVolume>()) {
        ctx.emplace<SoilVolume>(vol_config);
    } else {
        ctx.erase<SoilVolume>();
        ctx.emplace<SoilVolume>(vol_config);
    }

    // Initialize plant spatial index
    if (!ctx.contains<PlantSpatialIndex>()) {
        ctx.emplace<PlantSpatialIndex>(config.plant_spatial_cell_size);
    } else {
        ctx.erase<PlantSpatialIndex>();
        ctx.emplace<PlantSpatialIndex>(config.plant_spatial_cell_size);
    }

    // Initialize feeding statistics
    if (!ctx.contains<FeedingStatistics>()) {
        ctx.emplace<FeedingStatistics>();
    } else {
        ctx.erase<FeedingStatistics>();
        ctx.emplace<FeedingStatistics>();
    }

    // Initialize plant species registry
    if (!ctx.contains<PlantSpeciesRegistry>()) {
        ctx.emplace<PlantSpeciesRegistry>();
    } else {
        ctx.erase<PlantSpeciesRegistry>();
        ctx.emplace<PlantSpeciesRegistry>();
    }
}

void seed_initial_plants(entt::registry& registry, const EnvironmentConfig& config) {
    auto& terrain = registry.ctx().get<Terrain>();
    const auto* biome_map = registry.ctx().find<BiomeMap>();
    const auto* water_map = registry.ctx().find<WaterMap>();
    auto* species_registry = registry.ctx().find<PlantSpeciesRegistry>();

    std::mt19937 rng(config.plants.seed);
    std::uniform_real_distribution<double> dist_x(
        0.0, static_cast<double>(terrain.width() - 1) * terrain.cell_size());
    std::uniform_real_distribution<double> dist_z(
        0.0, static_cast<double>(terrain.height_cells() - 1) * terrain.cell_size());
    std::uniform_real_distribution<double> dist_energy(config.plants.min_initial_energy,
                                                       config.plants.max_initial_energy);
    std::uniform_real_distribution<double> jitter_radius(0.0, config.plant_spatial_cell_size);
    std::uniform_real_distribution<double> jitter_angle(0.0, 2.0 * std::numbers::pi_v<double>);
    std::uniform_real_distribution<double> energy_fraction(0.4, 0.9);

    std::size_t spawned = 0;
    std::size_t attempts = 0;
    constexpr std::size_t kMaxAttempts = 50000;

    while (spawned < config.plants.initial_count && attempts < kMaxAttempts) {
        ++attempts;
        const double base_x = dist_x(rng);
        const double base_z = dist_z(rng);
        const double angle = jitter_angle(rng);
        const double radius = jitter_radius(rng);
        const double x = base_x + std::cos(angle) * radius;
        const double z = base_z + std::sin(angle) * radius;
        const double y = terrain.height(x, z);
        const Vec3 normal = terrain.normal(x, z);

        if (!should_place_on_normal(normal)) {
            continue;
        }

        const BiomeId biome = biome_map != nullptr ? biome_map->sample(x, z) : BiomeId::Plains;
        double depth = 0.0;
        double shore_distance = std::numeric_limits<double>::infinity();
        WaterZone zone = WaterZone::Terrestrial;
        if (water_map != nullptr) {
            depth = water_map->depth(x, z);
            shore_distance = water_map->shore_distance(x, z);
            zone = classify_water_zone(*water_map, x, z);
        }

        const PlantSpecies* selected_species = nullptr;
        if (species_registry != nullptr) {
            selected_species = &select_species_for_location(*species_registry,
                                                            biome,
                                                            zone,
                                                            depth,
                                                            shore_distance,
                                                            rng);
            if (!species_allows_location(*selected_species, biome, zone, depth, shore_distance)) {
                continue;
            }
        }

        const entt::entity plant = registry.create();
        TransformComponent transform{};
        transform.position = Vec3{x, y, z};
        registry.emplace<TransformComponent>(plant, transform);

        PlantComponent plant_component{};
        if (selected_species != nullptr) {
            plant_component.species_id = selected_species->id;
            plant_component.max_energy = selected_species->max_energy;
            const double init_energy = selected_species->max_energy * energy_fraction(rng);
            plant_component.energy = std::clamp(init_energy, 2.0, selected_species->max_energy);
            plant_component.radius = selected_species->radius;
            plant_component.growth_rate = selected_species->growth_rate;
            plant_component.seed_interval = selected_species->seed_interval;
        } else {
            plant_component.energy = dist_energy(rng);
            plant_component.max_energy = config.plants.max_initial_energy * 1.25;
            plant_component.radius = 0.7;
            plant_component.growth_rate = 1.5;
            plant_component.seed_interval = 25.0;
        }
        plant_component.cleanup_delay = 8.0;
        registry.emplace<PlantComponent>(plant, plant_component);

        PlantSeedParams seed_params{};
        if (selected_species != nullptr) {
            seed_params.seed_min_energy = std::max(1.0, selected_species->max_energy * 0.6);
            seed_params.seed_cost = std::max(0.5, selected_species->max_energy * 0.25);
            seed_params.seed_radius = selected_species->seed_radius;
            seed_params.establish_probability = selected_species->establish_prob;
        } else {
            seed_params.seed_min_energy = plant_component.energy * 0.8;
            seed_params.seed_cost = plant_component.energy * 0.25;
            seed_params.seed_radius = 6.0;
            seed_params.establish_probability = 0.6;
        }
        registry.emplace<PlantSeedParams>(plant, seed_params);

        registry.emplace<NameComponent>(plant, NameComponent{.value = "plant"});
        ++spawned;
    }

    if (spawned < config.plants.initial_count) {
        spdlog::warn("Requested {} plants but only spawned {} after {} attempts",
                     config.plants.initial_count,
                     spawned,
                     attempts);
    } else {
        spdlog::info("Spawned {} plants across the terrain", spawned);
    }

    update_environment_stats(registry);
}

}  // namespace evolution::sim
