#pragma once

#include <cstddef>

#include <entt/entt.hpp>

#include "evolution/sim/environment/environment.h"

namespace evolution::sim {

/**
 * @brief Parameters controlling initial environment population and services.
 */
struct PlantBootstrapConfig {
    std::size_t initial_count{500};   ///< Number of plants spawned at startup.
    double min_initial_energy{8.0};   ///< Minimum initial plant energy.
    double max_initial_energy{18.0};  ///< Maximum initial plant energy.
    unsigned int seed{2025};          ///< Random seed for deterministic placement.
};

/**
 * @brief Bundles configuration for terrain, soil, biomes, water, and initial plants.
 */
struct EnvironmentConfig {
    TerrainConfig terrain{};           ///< Terrain generation parameters.
    SoilConfig soil{};                 ///< Soil simulation parameters.
    BiomeConfig biome{};               ///< Biome map generation parameters.
    WaterConfig water{};               ///< Water map generation parameters.
    PlantBootstrapConfig plants{};     ///< Initial plant spawning configuration.
    double plant_spatial_cell_size{4.0}; ///< Cell size for plant spatial index (meters).
};

/**
 * @brief Creates terrain, soil, and plant spatial index services in the registry context.
 *
 * @param registry entt::registry& Registry to populate with environment services.
 * @param config const EnvironmentConfig& Configuration describing environment setup.
 */
void initialize_environment(entt::registry& registry, const EnvironmentConfig& config);

/**
 * @brief Seeds plants across the terrain using deterministic random sampling.
 *
 * @param registry entt::registry& Registry receiving the plant entities.
 * @param config const EnvironmentConfig& Environment configuration used for terrain references.
 */
void seed_initial_plants(entt::registry& registry, const EnvironmentConfig& config);

}  // namespace evolution::sim



