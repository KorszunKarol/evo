#pragma once

#include <cstddef>

#include <entt/entt.hpp>

#include "evolution/sim/environment/environment.h"

namespace evolution::sim {

/**
 * @brief Selects which soil simulation backend is activated during environment initialization.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1)
 * @note This is an initialization-time decision. Production builds should prefer `Volume3D` to
 *       avoid allocating/updating the legacy 2D `SoilGrid` alongside `SoilVolume`.
 * @warning When set to `Volume3D`, callers must not assume `SoilGrid` exists in the registry
 *          context.
 * @notthreadsafe Intended for single-threaded setup only.
 */
enum class SoilMode {
    Legacy2D,  ///< Enable legacy 2D `SoilGrid` services and updates.
    Volume3D   ///< Enable 3D `SoilVolume` services (default) and skip legacy grid.
};

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
    SoilMode soil_mode{SoilMode::Volume3D}; ///< Selects legacy 2D vs 3D-only soil operation.
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


