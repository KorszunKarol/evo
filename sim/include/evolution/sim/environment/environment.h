#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>
#include <iostream>

#include <entt/entt.hpp>

#include "evolution/sim/components.h"
#include "evolution/sim/math_types.h"

namespace evolution::sim {

///
/// @brief Configuration for procedural terrain generation.
///
struct TerrainConfig {
    int width_cells{128};
    int height_cells{128};
    double cell_size{1.0};
    unsigned int seed{12345};

    double elevation_scale{10.0};
    double base_frequency{0.02};
    int octaves{4};
    double lacunarity{2.0};
    double gain{0.5};
};

///
/// @brief Represents the heightmap and derived data for the terrain.
///
class Terrain {
public:
    explicit Terrain(const TerrainConfig& config);

    [[nodiscard]] double height(double x, double z) const noexcept;
    [[nodiscard]] Vec3 normal(double x, double z) const noexcept;

    [[nodiscard]] double min_y() const noexcept { return min_height_; }
    [[nodiscard]] double max_y() const noexcept { return max_height_; }
    [[nodiscard]] int width() const noexcept { return width_cells_; }
    [[nodiscard]] int height_cells() const noexcept { return height_cells_; }
    [[nodiscard]] double cell_size() const noexcept { return cell_size_; }

private:
    [[nodiscard]] double sample_discrete(int ix, int iz) const noexcept;

    TerrainConfig config_;
    int width_cells_;
    int height_cells_;
    double cell_size_;
    double inv_cell_size_;
    std::vector<double> samples_;
    double min_height_;
    double max_height_;
};

///
/// @brief Configuration for soil grid.
///
struct SoilConfig {
    int width_cells{128};
    int height_cells{128};
    double cell_size{1.0};
    float baseline_nutrient{5.0F};
    float max_nutrient{100.0F};
    float diffusion_rate{0.1F};
    float regeneration_rate{0.5F};
};

// Forward declaration to avoid circular dependency
class BiomeMap;

///
/// @brief 2D grid of soil nutrients.
///
class SoilGrid {
public:
    explicit SoilGrid(const SoilConfig& config);

    [[nodiscard]] const SoilConfig& config() const noexcept { return config_; }

    [[nodiscard]] float at(int x, int z) const noexcept {
        if (x < 0 || x >= width_ || z < 0 || z >= height_) return config_.baseline_nutrient;
        return nutrients_[static_cast<std::size_t>(z) * width_ + static_cast<std::size_t>(x)];
    }

    [[nodiscard]] float& at(int x, int z) {
        // Bounds check or ensure valid usage? For perf, assume valid if internal.
        // But for public API safe access:
        static float dummy = 0.0f;
        if (x < 0 || x >= width_ || z < 0 || z >= height_) {
            dummy = config_.baseline_nutrient;
            return dummy;
        }
        return nutrients_[static_cast<std::size_t>(z) * width_ + static_cast<std::size_t>(x)];
    }

    [[nodiscard]] float sample(double x, double z) const noexcept;

    void diffuse(double dt) noexcept;
    void regenerate(double dt) noexcept;
    
    // New: Regenerate based on biome
    void regenerate_by_biome(double dt,
                             const BiomeMap& biome_map,
                             const std::array<float, 4>& regen_rates,
                             const std::array<float, 4>& baselines,
                             double climate_mult) noexcept;

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] double cell_size() const noexcept { return cell_size_; }
    [[nodiscard]] double mean_nutrient() const noexcept;

private:
    SoilConfig config_;
    int width_;
    int height_;
    double cell_size_;
    double inv_cell_size_;
    std::vector<float> nutrients_;
    std::vector<float> scratch_;
};

///
/// @brief Spatial index for plants to accelerate neighbor queries.
///
class PlantSpatialIndex {
public:
    explicit PlantSpatialIndex(double cell_size) noexcept;

    void clear() noexcept;
    void rebuild(entt::registry& registry);
    void insert(entt::entity entity, const Vec3& position);

    template <typename Func>
    void for_each_in_radius(entt::registry& registry,
                            const Vec3& position,
                            double radius,
                            Func&& func) const;

private:
    struct CellKey {
        int ix{0};
        int iz{0};

        [[nodiscard]] bool operator==(const CellKey& other) const noexcept {
            return ix == other.ix && iz == other.iz;
        }
    };

    struct CellHasher {
        [[nodiscard]] std::size_t operator()(const CellKey& cell) const noexcept {
            const std::uint64_t x = static_cast<std::uint64_t>(cell.ix) * 73856093u;
            const std::uint64_t z = static_cast<std::uint64_t>(cell.iz) * 83492791u;
            return static_cast<std::size_t>(x ^ z);
        }
    };

    double cell_size_;
    double inv_cell_size_;
    using Grid = std::unordered_map<CellKey, std::vector<entt::entity>, CellHasher>;
    Grid grid_;
};

///
/// @brief Aggregated statistics from the feeding system exported for telemetry.
///
struct FeedingStatistics {
    double energy_from_plants{0.0};     ///< Energy gained by herbivores eating plants.
    double energy_from_scavenging{0.0}; ///< Energy gained by carnivores eating corpses.
    double energy_from_hunting{0.0};    ///< Energy gained by carnivores eating live prey.
    double energy_transferred_last_tick{0.0}; // Legacy/Total
};

///
/// @brief Biome types.
///
enum class BiomeId : std::uint8_t {
    Plains = 0,
    Forest = 1,
    Wetland = 2,
    Alpine = 3
};

///
/// @brief Configuration for biome generation.
///
struct BiomeConfig {
    int width_cells{128};
    int height_cells{128};
    double cell_size{1.0};
    unsigned int seed{54321};
    
    int biome_count{3};          // 2=Plains/Forest, 3=+Wetland, 4=+Alpine
    double base_frequency{0.005};
    int octaves{3};
    double lacunarity{2.0};
    double gain{0.5};
    
    double elevation_bias{0.0};   // Influence of elevation on biome (0.0-1.0)
    double alpine_threshold{0.8}; // Normalized height above which Alpine is favored
};

///
/// @brief Map of biomes derived from noise and terrain.
///
class BiomeMap {
public:
    BiomeMap(const BiomeConfig& config, const Terrain& terrain);

    [[nodiscard]] BiomeId sample(double x, double z) const noexcept;
    [[nodiscard]] BiomeId sample_discrete(int ix, int iz) const noexcept;
    
    [[nodiscard]] int width() const noexcept { return width_cells_; }
    [[nodiscard]] int height() const noexcept { return height_cells_; }
    [[nodiscard]] double cell_size() const noexcept { return cell_size_; }

private:
    BiomeConfig config_;
    int width_cells_;
    int height_cells_;
    double cell_size_;
    double inv_cell_size_;
    std::vector<BiomeId> samples_;
};

///
/// @brief Water configuration.
///
struct WaterConfig {
    int width_cells{128};
    int height_cells{128};
    double cell_size{1.0};
    unsigned int seed{1337};
    
    double water_level_percentile{0.3};
    double min_flow_accumulation{5.0};
    double river_width_sigma{1.0};
    double river_depth_scale{2.0};
    double shore_band_max{2.0}; // Distance for shoreline zone
};

///
/// @brief Map of water depths and flow.
///
class WaterMap {
public:
    WaterMap(const WaterConfig& config, const Terrain& terrain);

    [[nodiscard]] double depth(double x, double z) const noexcept;
    [[nodiscard]] double shore_distance(double x, double z) const noexcept;
    
    [[nodiscard]] bool is_water(double x, double z) const noexcept {
        return depth(x, z) > 0.001;
    }

    [[nodiscard]] int width() const noexcept { return width_cells_; }
    [[nodiscard]] int height() const noexcept { return height_cells_; }
    [[nodiscard]] double cell_size() const noexcept { return cell_size_; }
    [[nodiscard]] double water_level() const noexcept { return water_level_; }
    [[nodiscard]] double shore_band_max() const noexcept { return shore_band_max_; }

private:
    void compute_flow_accumulation(const Terrain& terrain);
    void compute_shoreline_distances();
    
    [[nodiscard]] double depth_discrete(int ix, int iz) const noexcept;
    [[nodiscard]] double shore_distance_discrete(int ix, int iz) const noexcept;

    WaterConfig config_;
    int width_cells_;
    int height_cells_;
    double cell_size_;
    double inv_cell_size_;
    double water_level_;
    double shore_band_max_;
    
    std::vector<double> depths_;
    std::vector<double> shore_distances_;
    std::vector<double> flow_accumulation_;
};

///
/// @brief Plant Species Definition.
///
struct PlantSpecies {
    std::uint8_t id{0};
    double growth_rate{1.0};
    double max_energy{10.0};
    double radius{0.5};
    double seed_interval{20.0};
    double seed_radius{5.0};
    double establish_prob{0.5};
    
    // Environmental Constraints
    std::uint8_t biome_mask{0xFF}; // Bitmask of allowed BiomeIds
    std::uint8_t zone_mask{0xFF};  // Bitmask of allowed WaterZones
    
    double aquatic_depth_min{0.0};
    double shoreline_depth_max{1.0};
    double shoreline_distance_min{0.0};
    double shoreline_distance_max{10.0};
};

///
/// @brief Registry of plant species.
///
class PlantSpeciesRegistry {
public:
    PlantSpeciesRegistry();

    [[nodiscard]] const PlantSpecies& get(std::uint8_t id) const noexcept;
    [[nodiscard]] std::size_t count() const noexcept { return active_count_; }

private:
    std::array<PlantSpecies, 8> species_; // Max 8 species for now
    std::size_t active_count_{0};
};

///
/// @brief Water zones classification.
///
enum class WaterZone : std::uint8_t {
    Aquatic = 0,    // Deep water
    Shoreline = 1,  // Shallow water or near water
    Terrestrial = 2 // Dry land
};

[[nodiscard]] WaterZone classify_water_zone(const WaterMap& water_map, double x, double z) noexcept;

[[nodiscard]] bool species_allows_location(const PlantSpecies& species,
                                           BiomeId biome,
                                           WaterZone zone,
                                           double depth,
                                           double shore_distance) noexcept;

///
/// @brief Global environment statistics.
///
struct EnvironmentStats {
    std::array<double, 4> biome_biomass{0.0, 0.0, 0.0, 0.0};
    std::array<int, 8> species_counts{0};
    double total_biomass{0.0};
    double soil_mean{0.0};
    double land_fraction{1.0};
    
    void reset() noexcept;
};

void update_environment_stats(entt::registry& registry);

// Template implementation

template <typename Func>
void evolution::sim::PlantSpatialIndex::for_each_in_radius(entt::registry& registry,
                                                           const Vec3& position,
                                                           double radius,
                                                           Func&& func) const {
    if (grid_.empty()) {
        // std::cout << "DEBUG: Grid empty" << std::endl;
        return;
    }

    const double radius_sq = radius * radius;
    const int min_ix = static_cast<int>(std::floor(position.x * inv_cell_size_ - radius * inv_cell_size_));
    const int max_ix = static_cast<int>(std::floor(position.x * inv_cell_size_ + radius * inv_cell_size_));
    const int min_iz = static_cast<int>(std::floor(position.z * inv_cell_size_ - radius * inv_cell_size_));
    const int max_iz = static_cast<int>(std::floor(position.z * inv_cell_size_ + radius * inv_cell_size_));

    // std::cout << "DEBUG: Searching " << position.x << "," << position.z << " r=" << radius 
    //           << " range=[" << min_ix << "," << max_ix << "]x[" << min_iz << "," << max_iz << "]" << std::endl;

    for (int iz = min_iz; iz <= max_iz; ++iz) {
        for (int ix = min_ix; ix <= max_ix; ++ix) {
            const CellKey key{ix, iz};
            const auto it = grid_.find(key);
            if (it == grid_.end()) {
                continue;
            }

            // std::cout << "DEBUG: Found cell " << ix << "," << iz << " count=" << it->second.size() << std::endl;

            for (const entt::entity entity : it->second) {
                if (!registry.valid(entity)) {
                    continue;
                }
                // Transform component is guaranteed if inserted via rebuild/insert? 
                // But might be removed? Unlikely in this loop.
                // Use View from registry to be safe or get?
                const auto* transform = registry.try_get<TransformComponent>(entity);
                if (transform == nullptr) {
                    continue;
                }

                const double dx = transform->position.x - position.x;
                const double dz = transform->position.z - position.z;
                const double dist_sq = dx * dx + dz * dz;

                if (dist_sq <= radius_sq) {
                    func(entity, dist_sq);
                }
            }
        }
    }
}

}  // namespace evolution::sim
