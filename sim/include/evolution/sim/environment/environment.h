#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

#include "evolution/sim/components.h"
#include "evolution/sim/math_types.h"

namespace evolution::sim {

/**
 * @brief Configuration parameters used when generating the terrain heightfield.
 *
 * @note The terrain is generated deterministically from the supplied seed so that headless
 *       and viewer binaries remain in sync even when initialized independently.
 */
struct TerrainConfig {
    int width_cells{512};           ///< Number of samples along the X axis (must be >= 2).
    int height_cells{512};          ///< Number of samples along the Z axis (must be >= 2).
    double cell_size{1.0};          ///< World-space distance between neighboring samples in meters.
    double elevation_scale{20.0};   ///< Maximum height displacement applied to noise output.
    unsigned int seed{1337};        ///< Seed controlling deterministic noise generation.
    int octaves{5};                 ///< Number of fractal noise octaves to accumulate.
    double base_frequency{0.005};   ///< Base frequency of the first octave in inverse meters.
    double lacunarity{2.0};         ///< Frequency multiplier applied per octave.
    double gain{0.5};               ///< Amplitude multiplier applied per octave.
};

/**
 * @brief Scalar field representing the simulation terrain as a deterministic height map.
 *
 * @details The terrain provides continuous height and normal queries by bilinearly
 *          interpolating precomputed samples. Normals rely on central differences and
 *          therefore remain stable even on steep slopes when queried with small deltas.
 */
class Terrain {
public:
    /**
     * @brief Constructs the heightfield using the supplied configuration.
     *
     * @param config const TerrainConfig& User-specified generator parameters.
     */
    explicit Terrain(const TerrainConfig& config);

    /**
     * @brief Returns terrain height in meters at the given world-space position.
     *
     * @param x double World-space X coordinate in meters.
     * @param z double World-space Z coordinate in meters.
     * @return double Height in meters above the world origin.
     * @warning Heights queried outside the generated domain are clamped to the closest edge.
     * @complexity O(1)
     */
    [[nodiscard]] double height(double x, double z) const noexcept;

    /**
     * @brief Computes an upward-facing unit normal approximated from the heightfield gradient.
     *
     * @param x double World-space X coordinate.
     * @param z double World-space Z coordinate.
     * @return Vec3 Unit-length surface normal.
     * @complexity O(1)
     */
    [[nodiscard]] Vec3 normal(double x, double z) const noexcept;

    /**
     * @brief Minimum sampled height value.
     *
     * @return double Lowest terrain elevation encountered during generation.
     */
    [[nodiscard]] double min_y() const noexcept { return min_height_; }

    /**
     * @brief Maximum sampled height value.
     *
     * @return double Highest terrain elevation encountered during generation.
     */
    [[nodiscard]] double max_y() const noexcept { return max_height_; }

    /**
     * @brief Width of the heightfield in cells.
     */
    [[nodiscard]] int width() const noexcept { return width_cells_; }

    /**
     * @brief Height of the heightfield in cells.
     */
    [[nodiscard]] int height_cells() const noexcept { return height_cells_; }

    /**
     * @brief Grid spacing along X and Z axes.
     */
    [[nodiscard]] double cell_size() const noexcept { return cell_size_; }

    /**
     * @brief Provides read-only access to the raw height samples.
     *
     * @return const std::vector<double>& Row-major array sized width_cells * height_cells.
     */
    [[nodiscard]] const std::vector<double>& samples() const noexcept { return samples_; }

private:
    double sample_discrete(int ix, int iz) const noexcept;

    TerrainConfig config_{};
    int width_cells_{0};
    int height_cells_{0};
    double cell_size_{1.0};
    double inv_cell_size_{1.0};
    std::vector<double> samples_{};
    double min_height_{0.0};
    double max_height_{0.0};
};

/**
 * @brief Configuration describing soil nutrient simulation properties.
 */
struct SoilConfig {
    int width_cells{256};          ///< Grid width; typically terrain.width_cells / 2.
    int height_cells{256};         ///< Grid height; typically terrain.height_cells / 2.
    double cell_size{2.0};         ///< World size per cell (meters).
    float max_nutrient{10.0F};     ///< Upper bound for stored nutrients per cell.
    float diffusion_rate{0.5F};    ///< Diffusion coefficient (units per second).
    float regeneration_rate{0.05F};///< Baseline nutrient regeneration per second.
    float baseline_nutrient{4.0F}; ///< Floor value maintained by regeneration.
};

// Forward declaration
class BiomeMap;

/**
 * @brief 2D scalar field storing soil nutrient concentration sampled by plants.
 *
 * @details The soil grid maintains an internal scratch buffer used during diffusion
 *          to avoid allocations in the hot path. Diffusion uses a simple five-point
 *          stencil with reflective boundaries.
 */
class SoilGrid {
public:
    /**
     * @brief Constructs a soil grid with the supplied configuration.
     *
     * @param config const SoilConfig& Grid sizing and simulation parameters.
     */
    explicit SoilGrid(const SoilConfig& config);

    /**
     * @brief Returns the configuration used to create the soil grid.
     */
    [[nodiscard]] const SoilConfig& config() const noexcept { return config_; }

    /**
     * @brief Number of cells along the X axis.
     */
    [[nodiscard]] int width() const noexcept { return width_; }

    /**
     * @brief Number of cells along the Z axis.
     */
    [[nodiscard]] int height() const noexcept { return height_; }

    /**
     * @brief World-space size of each cell.
     */
    [[nodiscard]] double cell_size() const noexcept { return cell_size_; }

    /**
     * @brief Returns the nutrient value at discrete grid coordinates.
     *
     * @param ix int Cell index along X.
     * @param iz int Cell index along Z.
     * @return float& Mutable reference to the cell value.
     * @warning No bounds checking is performed; caller must clamp indices.
     */
    [[nodiscard]] float& at(int ix, int iz) noexcept {
        return nutrients_[static_cast<std::size_t>(iz) * width_ + static_cast<std::size_t>(ix)];
    }

    /**
     * @brief Returns the nutrient value at discrete grid coordinates (const overload).
     *
     * @param ix int Cell index along X.
     * @param iz int Cell index along Z.
     * @return float Nutrient value stored in the cell.
     */
    [[nodiscard]] float at(int ix, int iz) const noexcept {
        return nutrients_[static_cast<std::size_t>(iz) * width_ + static_cast<std::size_t>(ix)];
    }

    /**
     * @brief Samples nutrient concentration at an arbitrary world position.
     *
     * @param x double World-space X coordinate in meters.
     * @param z double World-space Z coordinate in meters.
     * @return float Bilinearly interpolated nutrient value.
     * @complexity O(1)
     */
    [[nodiscard]] float sample(double x, double z) const noexcept;

    /**
     * @brief Performs a single diffusion step using a five-point stencil.
     *
     * @param dt double Simulation timestep in seconds.
     * @note Diffusion conserves mass except for regeneration and clamping.
     */
    void diffuse(double dt) noexcept;

    /**
     * @brief Regenerates nutrients toward the configured baseline.
     *
     * @param dt double Simulation timestep in seconds.
     */
    void regenerate(double dt) noexcept;

    /**
     * @brief Regenerates nutrients with biome-specific rates and climate multiplier.
     *
     * @param dt double Simulation timestep in seconds.
     * @param biome_map const BiomeMap& Biome map for querying biome at each cell.
     * @param regen_rates const std::array<float, 4>& Regeneration rates per biome [Plains, Forest, Wetland, Alpine].
     * @param baselines const std::array<float, 4>& Baseline nutrients per biome.
     * @param climate_mult double Climate multiplier (day/night/seasonal).
     */
    void regenerate_by_biome(double dt,
                             const BiomeMap& biome_map,
                             const std::array<float, 4>& regen_rates,
                             const std::array<float, 4>& baselines,
                             double climate_mult) noexcept;

    /**
     * @brief Computes the arithmetic mean of nutrient values.
     *
     * @return double Mean nutrient level across the grid.
     */
    [[nodiscard]] double mean_nutrient() const noexcept;

private:
    SoilConfig config_{};
    int width_{0};
    int height_{0};
    double cell_size_{1.0};
    double inv_cell_size_{1.0};
    std::vector<float> nutrients_{};
    std::vector<float> scratch_{};
};

/**
 * @brief Lightweight spatial hash tailored for planar queries against plants.
 *
 * @details The hash discretizes the XZ plane using uniform square cells and
 *          stores entity identifiers for quick radius queries.
 */
class PlantSpatialIndex {
public:
    /**
     * @brief Constructs the index with a specific cell size.
     *
     * @param cell_size double Cell edge length measured in meters.
     */
    explicit PlantSpatialIndex(double cell_size = 4.0) noexcept;

    /**
     * @brief Clears all stored entries.
     */
    void clear() noexcept;

    /**
     * @brief Rebuilds the hash from the current set of plant entities.
     *
     * @param registry entt::registry& ECS registry containing plant data.
     */
    void rebuild(entt::registry& registry);

    /**
     * @brief Invokes a callback for each plant located within the supplied radius.
     *
     * @tparam Func Callable invoked with (entt::entity, double distance_squared).
     * @param registry entt::registry& Registry used to obtain component data.
     * @param position const Vec3& Query position in meters.
     * @param radius double Search radius in meters.
     * @param func Func&& Callback invoked for candidate plants that are still alive.
     */
    template <typename Func>
    void for_each_in_radius(entt::registry& registry,
                            const Vec3& position,
                            double radius,
                            Func&& func) const;

    /**
     * @brief Cell key used internally by the hash map.
     */
    struct CellKey {
        int ix{0};
        int iz{0};

        [[nodiscard]] bool operator==(const CellKey& other) const noexcept {
            return ix == other.ix && iz == other.iz;
        }
    };

private:
    struct CellHasher {
        [[nodiscard]] std::size_t operator()(const CellKey& cell) const noexcept {
            const std::uint64_t x = static_cast<std::uint64_t>(cell.ix) * 73856093u;
            const std::uint64_t z = static_cast<std::uint64_t>(cell.iz) * 83492791u;
            return static_cast<std::size_t>(x ^ z);
        }
    };

    void insert(entt::entity entity, const Vec3& position);

    double cell_size_{4.0};
    double inv_cell_size_{0.25};
    std::unordered_map<CellKey, std::vector<entt::entity>, CellHasher> grid_{};
};

/**
 * @brief Aggregated statistics from the feeding system exported for telemetry.
 */
struct FeedingStatistics {
    double energy_transferred_last_tick{0.0}; ///< Total herbivore energy gained during the last update step.
};

/**
 * @brief Water proximity zone for plant spawning.
 */
enum class WaterZone : std::uint8_t {
    Aquatic = 0,     ///< Deep water (depth >= threshold).
    Shoreline = 1,   ///< Shallow water or near shore.
    Terrestrial = 2 ///< Dry land.
};

/**
 * @brief Plant species definition with biome and water zone preferences.
 */
struct PlantSpecies {
    std::uint8_t id{0};                    ///< Unique species ID (0-4).
    double growth_rate{2.0};               ///< Energy/sec from soil.
    double max_energy{20.0};                ///< Maximum energy capacity.
    double radius{0.6};                     ///< Edible/contact radius (m).
    double seed_interval{20.0};             ///< Seconds between seed attempts.
    double seed_radius{6.0};                ///< Spawn radius (m).
    double establish_prob{0.65};            ///< Success probability.
    std::uint8_t biome_mask{0xFF};         ///< Bitmask of allowed biomes (bits 0-3).
    std::uint8_t zone_mask{0x04};           ///< Bitmask of allowed zones (bit 0=aquatic, 1=shoreline, 2=terrestrial).
    double aquatic_depth_min{0.5};          ///< Minimum depth for aquatic zone (m).
    double shoreline_depth_max{1.2};        ///< Maximum depth for shoreline zone (m).
    double shoreline_distance_min{0.0};     ///< Minimum shore distance for shoreline zone (m).
    double shoreline_distance_max{8.0};     ///< Maximum shore distance for shoreline zone (m).
};

/**
 * @brief Registry of plant species definitions.
 *
 * @details Provides species lookup and default species definitions for MVP.
 */
class PlantSpeciesRegistry {
public:
    /**
     * @brief Constructs registry with default MVP species.
     */
    PlantSpeciesRegistry();

    /**
     * @brief Returns species definition by ID.
     *
     * @param id std::uint8_t Species ID (0-4).
     * @return const PlantSpecies& Species definition.
     * @warning Returns default species if ID is invalid.
     */
    [[nodiscard]] const PlantSpecies& get(std::uint8_t id) const noexcept;

    /**
     * @brief Number of registered species.
     */
    [[nodiscard]] std::size_t count() const noexcept { return species_.size(); }

private:
    std::array<PlantSpecies, 5> species_{};
};

/**
 * @brief Biome type identifiers.
 */
enum class BiomeId : std::uint8_t {
    Plains = 0,   ///< Open grassland biome.
    Forest = 1,   ///< Dense forest biome.
    Wetland = 2,  ///< Marsh/swamp biome.
    Alpine = 3    ///< High elevation biome (optional).
};

/**
 * @brief Configuration for biome map generation.
 */
struct BiomeConfig {
    int width_cells{512};           ///< Grid width (matches terrain).
    int height_cells{512};          ///< Grid height (matches terrain).
    double cell_size{1.0};          ///< World-space cell size (matches terrain).
    unsigned int seed{1337};         ///< Seed for deterministic generation.
    int biome_count{3};              ///< Number of biomes to generate (2-4).
    int octaves{4};                  ///< Noise octaves for biome distribution.
    double base_frequency{0.003};    ///< Base frequency for biome noise.
    double lacunarity{2.0};          ///< Frequency multiplier per octave.
    double gain{0.5};                ///< Amplitude multiplier per octave.
    double elevation_bias{0.3};     ///< Elevation influence on alpine biome (0-1).
    double alpine_threshold{0.7};    ///< Normalized elevation threshold for alpine.
};

/**
 * @brief Deterministic biome map derived from noise patterns.
 *
 * @details Provides biome classification at any world position. Biomes are
 *          generated using multi-octave noise and optional elevation bias.
 */
class BiomeMap {
public:
    /**
     * @brief Constructs the biome map from configuration and terrain reference.
     *
     * @param config const BiomeConfig& Generation parameters.
     * @param terrain const Terrain& Terrain reference for elevation queries.
     */
    explicit BiomeMap(const BiomeConfig& config, const Terrain& terrain);

    /**
     * @brief Samples the biome ID at a world position.
     *
     * @param x double World-space X coordinate.
     * @param z double World-space Z coordinate.
     * @return BiomeId The biome classification.
     * @complexity O(1) - bilinear interpolation of precomputed samples.
     */
    [[nodiscard]] BiomeId sample(double x, double z) const noexcept;

    /**
     * @brief Width of the biome grid in cells.
     */
    [[nodiscard]] int width() const noexcept { return width_cells_; }

    /**
     * @brief Height of the biome grid in cells.
     */
    [[nodiscard]] int height() const noexcept { return height_cells_; }

    /**
     * @brief Grid spacing along X and Z axes.
     */
    [[nodiscard]] double cell_size() const noexcept { return cell_size_; }

private:
    BiomeId sample_discrete(int ix, int iz) const noexcept;

    BiomeConfig config_{};
    int width_cells_{0};
    int height_cells_{0};
    double cell_size_{1.0};
    double inv_cell_size_{1.0};
    std::vector<BiomeId> samples_{};
};

/**
 * @brief Configuration for water map generation.
 */
struct WaterConfig {
    int width_cells{512};           ///< Grid width (matches terrain).
    int height_cells{512};          ///< Grid height (matches terrain).
    double cell_size{1.0};          ///< World-space cell size (matches terrain).
    unsigned int seed{1337};        ///< Seed for deterministic river generation.
    double water_level_percentile{0.25}; ///< Percentile of terrain height for lake level (0-1).
    double min_flow_accumulation{500.0};  ///< Minimum flow accumulation for rivers.
    double river_depth_scale{0.8};       ///< Depth multiplier for river channels.
    double river_width_sigma{1.5};       ///< Gaussian width for river widening (cells).
    double shore_band_max{2.0};          ///< Maximum depth considered shoreline (meters).
};

/**
 * @brief Water depth and shoreline information derived from terrain.
 *
 * @details Combines height-based lakes with flow-accumulation rivers.
 *          Provides depth queries and shoreline distance calculations.
 */
class WaterMap {
public:
    /**
     * @brief Constructs the water map from configuration and terrain reference.
     *
     * @param config const WaterConfig& Generation parameters.
     * @param terrain const Terrain& Terrain reference for height queries.
     */
    explicit WaterMap(const WaterConfig& config, const Terrain& terrain);

    /**
     * @brief Returns water depth in meters at a world position.
     *
     * @param x double World-space X coordinate.
     * @param z double World-space Z coordinate.
     * @return double Depth in meters (0 if dry land).
     * @complexity O(1) - bilinear interpolation.
     */
    [[nodiscard]] double depth(double x, double z) const noexcept;

    /**
     * @brief Returns distance to nearest water edge in meters.
     *
     * @param x double World-space X coordinate.
     * @param z double World-space Z coordinate.
     * @return double Distance to shoreline (0 if in water).
     * @complexity O(1) - approximate distance from precomputed field.
     */
    [[nodiscard]] double shore_distance(double x, double z) const noexcept;

    /**
     * @brief Checks if a position is underwater.
     *
     * @param x double World-space X coordinate.
     * @param z double World-space Z coordinate.
     * @return bool True if depth > 0.
     */
    [[nodiscard]] bool is_water(double x, double z) const noexcept {
        return depth(x, z) > 0.0;
    }

    /**
     * @brief Returns the global water level used for lake generation.
     */
    [[nodiscard]] double water_level() const noexcept { return water_level_; }

    /**
     * @brief Returns the configured shoreline band threshold.
     */
    [[nodiscard]] double shore_band_max() const noexcept { return shore_band_max_; }

    /**
     * @brief Width of the water grid in cells.
     */
    [[nodiscard]] int width() const noexcept { return width_cells_; }

    /**
     * @brief Height of the water grid in cells.
     */
    [[nodiscard]] int height() const noexcept { return height_cells_; }

    /**
     * @brief Grid spacing along X and Z axes.
     */
    [[nodiscard]] double cell_size() const noexcept { return cell_size_; }

private:
    double depth_discrete(int ix, int iz) const noexcept;
    double shore_distance_discrete(int ix, int iz) const noexcept;
    void compute_flow_accumulation(const Terrain& terrain);
    void compute_shoreline_distances();

    WaterConfig config_{};
    int width_cells_{0};
    int height_cells_{0};
    double cell_size_{1.0};
    double inv_cell_size_{1.0};
    double water_level_{0.0};
    double shore_band_max_{0.0};
    std::vector<double> depths_{};
    std::vector<double> shore_distances_{};
    std::vector<double> flow_accumulation_{};
};

/**
 * @brief Aggregated environment telemetry updated by stats systems/viewer.
 */
struct EnvironmentStats {
    std::array<double, 4> biome_biomass{};
    std::array<std::uint32_t, 5> species_counts{};
    double total_biomass{0.0};
    double soil_mean{0.0};
    double land_fraction{1.0};

    void reset() noexcept;
};

[[nodiscard]] WaterZone classify_water_zone(const WaterMap& water_map, double x, double z) noexcept;
[[nodiscard]] bool species_allows_location(const PlantSpecies& species,
                                           BiomeId biome,
                                           WaterZone zone,
                                           double depth,
                                           double shore_distance) noexcept;
void update_environment_stats(entt::registry& registry);

}  // namespace evolution::sim

template <typename Func>
void evolution::sim::PlantSpatialIndex::for_each_in_radius(entt::registry& registry,
                                                           const Vec3& position,
                                                           double radius,
                                                           Func&& func) const {
    if (grid_.empty()) {
        return;
    }
    const double radius_sq = radius * radius;
    const int min_ix = static_cast<int>(std::floor(position.x * inv_cell_size_ - radius * inv_cell_size_));
    const int max_ix = static_cast<int>(std::floor(position.x * inv_cell_size_ + radius * inv_cell_size_));
    const int min_iz = static_cast<int>(std::floor(position.z * inv_cell_size_ - radius * inv_cell_size_));
    const int max_iz = static_cast<int>(std::floor(position.z * inv_cell_size_ + radius * inv_cell_size_));

    for (int iz = min_iz; iz <= max_iz; ++iz) {
        for (int ix = min_ix; ix <= max_ix; ++ix) {
            const CellKey key{ix, iz};
            const auto it = grid_.find(key);
            if (it == grid_.end()) {
                continue;
            }
            for (const entt::entity entity : it->second) {
                if (!registry.valid(entity)) {
                    continue;
                }
                const auto* transform = registry.try_get<TransformComponent>(entity);
                const auto* plant = registry.try_get<struct PlantComponent>(entity);
                if (transform == nullptr || plant == nullptr || !plant->alive) {
                    continue;
                }
                const Vec3 delta = transform->position - position;
                const double dist_sq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
                if (dist_sq <= radius_sq) {
                    func(entity, dist_sq);
                }
            }
        }
    }
}


