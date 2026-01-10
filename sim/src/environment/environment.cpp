#include "evolution/sim/environment/environment.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"

namespace evolution::sim {

namespace {

constexpr double kEpsilon = 1e-5;

[[nodiscard]] double fade(double t) noexcept {
    // Quintic smoothstep used by Perlin noise.
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

[[nodiscard]] double lerp(double a, double b, double t) noexcept {
    return a + (b - a) * t;
}

[[nodiscard]] double hash_value(int x, int y, unsigned int seed) noexcept {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u
                      + static_cast<std::uint32_t>(y) * 668265263u
                      + static_cast<std::uint32_t>(seed) * 69069u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= (h >> 16u);
    return static_cast<double>(h & 0xFFFFFFu) / static_cast<double>(0xFFFFFFu);
}

[[nodiscard]] double value_noise(double x, double y, unsigned int seed) noexcept {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const double sx = fade(x - static_cast<double>(x0));
    const double sy = fade(y - static_cast<double>(y0));

    const double n0 = lerp(hash_value(x0, y0, seed), hash_value(x1, y0, seed), sx);
    const double n1 = lerp(hash_value(x0, y1, seed), hash_value(x1, y1, seed), sx);
    return lerp(n0, n1, sy);
}

[[nodiscard]] double fractal_noise(double x,
                                   double y,
                                   const TerrainConfig& config) noexcept {
    double amplitude = 1.0;
    double frequency = config.base_frequency;
    double sum = 0.0;
    double normalization = 0.0;

    for (int octave = 0; octave < config.octaves; ++octave) {
        const double sample = value_noise(x * frequency, y * frequency, config.seed + octave);
        sum += sample * amplitude;
        normalization += amplitude;
        amplitude *= config.gain;
        frequency *= config.lacunarity;
    }

    if (normalization <= kEpsilon) {
        return 0.0;
    }
    return sum / normalization;
}

[[nodiscard]] Vec3 compute_central_normal(const Terrain& terrain,
                                          double x,
                                          double z,
                                          double delta) noexcept {
    const double hx1 = terrain.height(x + delta, z);
    const double hx0 = terrain.height(x - delta, z);
    const double hz1 = terrain.height(x, z + delta);
    const double hz0 = terrain.height(x, z - delta);

    const Vec3 dx{2.0 * delta, hx1 - hx0, 0.0};
    const Vec3 dz{0.0, hz1 - hz0, 2.0 * delta};
    Vec3 normal{
        dz.y * dx.z - dz.z * dx.y,
        dz.z * dx.x - dz.x * dx.z,
        dz.x * dx.y - dz.y * dx.x,
    };
    const double length_sq = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
    if (length_sq <= kEpsilon) {
        return Vec3{0.0, 1.0, 0.0};
    }
    const double inv_len = 1.0 / std::sqrt(length_sq);
    normal.x *= inv_len;
    normal.y *= inv_len;
    normal.z *= inv_len;
    return normal;
}

}  // namespace

Terrain::Terrain(const TerrainConfig& config)
    : config_(config),
      width_cells_(std::max(config.width_cells, 2)),
      height_cells_(std::max(config.height_cells, 2)),
      cell_size_(std::max(config.cell_size, 1e-3)),
      inv_cell_size_(1.0 / cell_size_),
      samples_(static_cast<std::size_t>(width_cells_ * height_cells_), 0.0),
      min_height_(std::numeric_limits<double>::max()),
      max_height_(std::numeric_limits<double>::lowest()) {
    for (int z = 0; z < height_cells_; ++z) {
        for (int x = 0; x < width_cells_; ++x) {
            const double world_x = static_cast<double>(x) * cell_size_;
            const double world_z = static_cast<double>(z) * cell_size_;
            const double noise = fractal_noise(world_x, world_z, config_);
            const double height = (noise * 2.0 - 1.0) * config_.elevation_scale;
            const std::size_t index = static_cast<std::size_t>(z) * width_cells_
                                      + static_cast<std::size_t>(x);
            samples_[index] = height;
            min_height_ = std::min(min_height_, height);
            max_height_ = std::max(max_height_, height);
        }
    }
}

double Terrain::sample_discrete(int ix, int iz) const noexcept {
    ix = std::clamp(ix, 0, width_cells_ - 1);
    iz = std::clamp(iz, 0, height_cells_ - 1);
    return samples_[static_cast<std::size_t>(iz) * width_cells_
                    + static_cast<std::size_t>(ix)];
}

double Terrain::height(double x, double z) const noexcept {
    const double fx = std::clamp(x * inv_cell_size_, 0.0, static_cast<double>(width_cells_ - 1));
    const double fz = std::clamp(z * inv_cell_size_, 0.0, static_cast<double>(height_cells_ - 1));

    const int ix0 = static_cast<int>(std::floor(fx));
    const int iz0 = static_cast<int>(std::floor(fz));
    const int ix1 = ix0 + 1;
    const int iz1 = iz0 + 1;

    const double sx = fx - static_cast<double>(ix0);
    const double sz = fz - static_cast<double>(iz0);

    const double h00 = sample_discrete(ix0, iz0);
    const double h10 = sample_discrete(ix1, iz0);
    const double h01 = sample_discrete(ix0, iz1);
    const double h11 = sample_discrete(ix1, iz1);

    const double tx0 = lerp(h00, h10, sx);
    const double tx1 = lerp(h01, h11, sx);
    return lerp(tx0, tx1, sz);
}

Vec3 Terrain::normal(double x, double z) const noexcept {
    const double delta = cell_size_;
    return compute_central_normal(*this, x, z, delta);
}

SoilGrid::SoilGrid(const SoilConfig& config)
    : config_(config),
      width_(std::max(config.width_cells, 2)),
      height_(std::max(config.height_cells, 2)),
      cell_size_(std::max(config.cell_size, 1e-3)),
      inv_cell_size_(1.0 / cell_size_),
      nutrients_(static_cast<std::size_t>(width_ * height_), config.baseline_nutrient),
      scratch_(nutrients_) {}

float SoilGrid::sample(double x, double z) const noexcept {
    if (!std::isfinite(x) || !std::isfinite(z) || nutrients_.empty()) {
        return config_.baseline_nutrient;
    }

    const double fx = std::clamp(x * inv_cell_size_, 0.0, static_cast<double>(width_ - 1));
    const double fz = std::clamp(z * inv_cell_size_, 0.0, static_cast<double>(height_ - 1));
    const int ix0 = static_cast<int>(std::floor(fx));
    const int iz0 = static_cast<int>(std::floor(fz));
    const int ix1 = std::min(ix0 + 1, width_ - 1);
    const int iz1 = std::min(iz0 + 1, height_ - 1);
    const double sx = fx - static_cast<double>(ix0);
    const double sz = fz - static_cast<double>(iz0);
    const float n00 = at(ix0, iz0);
    const float n10 = at(ix1, iz0);
    const float n01 = at(ix0, iz1);
    const float n11 = at(ix1, iz1);
    const float tx0 = static_cast<float>(lerp(n00, n10, sx));
    const float tx1 = static_cast<float>(lerp(n01, n11, sx));
    return static_cast<float>(lerp(tx0, tx1, sz));
}

void SoilGrid::diffuse(double dt) noexcept {
    const float rate = config_.diffusion_rate * static_cast<float>(dt);
    if (rate <= 0.0F) {
        return;
    }

    auto clamp_index = [this](int value, int max_index) {
        return std::clamp(value, 0, max_index);
    };

    const int max_x = width_ - 1;
    const int max_z = height_ - 1;

    for (int z = 0; z < height_; ++z) {
        for (int x = 0; x < width_; ++x) {
            const int left = clamp_index(x - 1, max_x);
            const int right = clamp_index(x + 1, max_x);
            const int down = clamp_index(z - 1, max_z);
            const int up = clamp_index(z + 1, max_z);

            const float center = at(x, z);
            const float sum_neighbors = at(left, z) + at(right, z) + at(x, down) + at(x, up);
            const float laplacian = sum_neighbors - 4.0F * center;
            const float diffused = center + rate * laplacian;
            scratch_[static_cast<std::size_t>(z) * width_ + static_cast<std::size_t>(x)] =
                std::clamp(diffused, 0.0F, config_.max_nutrient);
        }
    }

    nutrients_.swap(scratch_);
}

void SoilGrid::regenerate(double dt) noexcept {
    const float regen = config_.regeneration_rate * static_cast<float>(dt);
    if (regen <= 0.0F) {
        return;
    }

    for (auto& cell : nutrients_) {
        cell = std::min(config_.max_nutrient, cell + regen);
        if (cell < config_.baseline_nutrient) {
            cell = std::min(config_.baseline_nutrient, cell + regen);
        }
    }
}

void SoilGrid::regenerate_by_biome(double dt,
                                   const BiomeMap& biome_map,
                                   const std::array<float, 4>& regen_rates,
                                   const std::array<float, 4>& baselines,
                                   double climate_mult) noexcept {
    const float climate_mult_f = static_cast<float>(climate_mult);
    if (climate_mult_f <= 0.0F || dt <= 0.0) {
        return;
    }

    for (int z = 0; z < height_; ++z) {
        for (int x = 0; x < width_; ++x) {
            const double world_x = static_cast<double>(x) * cell_size_;
            const double world_z = static_cast<double>(z) * cell_size_;
            const BiomeId biome = biome_map.sample(world_x, world_z);
            const int biome_idx = static_cast<int>(biome);
            const float regen_rate = (biome_idx >= 0 && biome_idx < 4) ? regen_rates[biome_idx] : regen_rates[0];
            const float baseline = (biome_idx >= 0 && biome_idx < 4) ? baselines[biome_idx] : baselines[0];

            float& cell = at(x, z);
            const float regen = regen_rate * climate_mult_f * static_cast<float>(dt);
            cell = std::min(config_.max_nutrient, cell + regen);
            if (cell < baseline) {
                cell = std::min(baseline, cell + regen);
            }
        }
    }
}

double SoilGrid::mean_nutrient() const noexcept {
    double sum = 0.0;
    for (const auto value : nutrients_) {
        sum += static_cast<double>(value);
    }
    return sum / static_cast<double>(nutrients_.size());
}

PlantSpatialIndex::PlantSpatialIndex(double cell_size) noexcept
    : cell_size_(std::max(cell_size, 0.1)),
      inv_cell_size_(1.0 / cell_size_) {}

void PlantSpatialIndex::clear() noexcept {
    grid_.clear();
}

void PlantSpatialIndex::rebuild(entt::registry& registry) {
    grid_.clear();
    auto view = registry.view<TransformComponent, struct PlantComponent>();
    std::size_t count = 0;
    for (auto entity : view) {
        const auto& transform = view.get<TransformComponent>(entity);
        const auto& plant = view.get<PlantComponent>(entity);
        if (!plant.alive) {
            continue;
        }
        insert(entity, transform.position);
        ++count;
    }
    spdlog::info("PlantSpatialIndex: Rebuilt with {} plants. Grid size: {}", count, grid_.size());
}

void PlantSpatialIndex::insert(entt::entity entity, const Vec3& position) {
    const int ix = static_cast<int>(std::floor(position.x * inv_cell_size_));
    const int iz = static_cast<int>(std::floor(position.z * inv_cell_size_));
    const CellKey key{ix, iz};
    grid_[key].push_back(entity);
    // spdlog::trace("PlantSpatialIndex: Inserted entity {} at ({}, {})", (uint32_t)entity, ix, iz);
}

namespace {

// Deterministic hash function for seed derivation
[[nodiscard]] std::uint64_t hash_string(const char* str) noexcept {
    std::uint64_t hash = 5381u;
    for (const char* p = str; *p != '\0'; ++p) {
        hash = ((hash << 5u) + hash) + static_cast<std::uint64_t>(*p);
    }
    return hash;
}

[[nodiscard]] unsigned int derive_seed(unsigned int base_seed, const char* tag) noexcept {
    const std::uint64_t tag_hash = hash_string(tag);
    return static_cast<unsigned int>((static_cast<std::uint64_t>(base_seed) ^ tag_hash) & 0xFFFFFFFFu);
}

[[nodiscard]] double fractal_noise_biome(double x,
                                        double y,
                                        const BiomeConfig& config) noexcept {
    double amplitude = 1.0;
    double frequency = config.base_frequency;
    double sum = 0.0;
    double normalization = 0.0;

    for (int octave = 0; octave < config.octaves; ++octave) {
        const unsigned int seed = derive_seed(config.seed, "biome") + static_cast<unsigned int>(octave);
        const double sample = value_noise(x * frequency, y * frequency, seed);
        sum += sample * amplitude;
        normalization += amplitude;
        amplitude *= config.gain;
        frequency *= config.lacunarity;
    }

    if (normalization <= kEpsilon) {
        return 0.0;
    }
    return sum / normalization;
}

}  // namespace

BiomeMap::BiomeMap(const BiomeConfig& config, const Terrain& terrain)
    : config_(config),
      width_cells_(std::max(config.width_cells, 2)),
      height_cells_(std::max(config.height_cells, 2)),
      cell_size_(std::max(config.cell_size, 1e-3)),
      inv_cell_size_(1.0 / cell_size_),
      samples_(static_cast<std::size_t>(width_cells_ * height_cells_), BiomeId::Plains) {
    const int biome_count = std::clamp(config.biome_count, 2, 4);
    const double terrain_min = terrain.min_y();
    const double terrain_max = terrain.max_y();
    const double terrain_range = std::max(terrain_max - terrain_min, kEpsilon);

    // Generate noise-based biome classification
    for (int z = 0; z < height_cells_; ++z) {
        for (int x = 0; x < width_cells_; ++x) {
            const double world_x = static_cast<double>(x) * cell_size_;
            const double world_z = static_cast<double>(z) * cell_size_;
            const double noise = fractal_noise_biome(world_x, world_z, config_);

            // Apply elevation bias for alpine biome if enabled
            double biome_value = noise;
            const double terrain_height = terrain.height(world_x, world_z);
            const double normalized_elevation = (terrain_height - terrain_min) / terrain_range;
            
            if (biome_count >= 4 && config_.elevation_bias > 0.0) {
                if (normalized_elevation >= config_.alpine_threshold) {
                    // Bias toward alpine at high elevations
                    biome_value = std::max(biome_value, normalized_elevation * config_.elevation_bias + noise * (1.0 - config_.elevation_bias));
                }
            }

            // Classify into biome based on quantized noise value
            BiomeId biome;
            if (biome_count == 2) {
                biome = (biome_value < 0.5) ? BiomeId::Plains : BiomeId::Forest;
            } else if (biome_count == 3) {
                if (biome_value < 0.33) {
                    biome = BiomeId::Plains;
                } else if (biome_value < 0.66) {
                    biome = BiomeId::Forest;
                } else {
                    biome = BiomeId::Wetland;
                }
            } else {  // biome_count == 4
                // Ensure alpine appears at high elevations even if noise is lower
                // This guarantees all 4 biomes appear when requested
                if (normalized_elevation >= 0.85) {
                    // Force alpine at very high elevations
                    biome = BiomeId::Alpine;
                } else if (biome_value < 0.25) {
                    biome = BiomeId::Plains;
                } else if (biome_value < 0.5) {
                    biome = BiomeId::Forest;
                } else if (biome_value < 0.75) {
                    biome = BiomeId::Wetland;
                } else {
                    biome = BiomeId::Alpine;
                }
            }

            const std::size_t index = static_cast<std::size_t>(z) * width_cells_ + static_cast<std::size_t>(x);
            samples_[index] = biome;
        }
    }

    // Apply smoothing pass (majority filter) for coherence
    std::vector<BiomeId> smoothed(samples_);
    for (int pass = 0; pass < 1; ++pass) {  // Single pass for performance
        for (int z = 1; z < height_cells_ - 1; ++z) {
            for (int x = 1; x < width_cells_ - 1; ++x) {
                std::array<int, 4> counts{0, 0, 0, 0};
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const std::size_t idx = static_cast<std::size_t>(z + dz) * width_cells_ + static_cast<std::size_t>(x + dx);
                        const int biome_idx = static_cast<int>(samples_[idx]);
                        if (biome_idx >= 0 && biome_idx < 4) {
                            counts[biome_idx]++;
                        }
                    }
                }
                const int max_idx = static_cast<int>(std::max_element(counts.begin(), counts.end()) - counts.begin());
                const std::size_t idx = static_cast<std::size_t>(z) * width_cells_ + static_cast<std::size_t>(x);
                smoothed[idx] = static_cast<BiomeId>(max_idx);
            }
        }
        samples_.swap(smoothed);
    }
}

BiomeId BiomeMap::sample_discrete(int ix, int iz) const noexcept {
    ix = std::clamp(ix, 0, width_cells_ - 1);
    iz = std::clamp(iz, 0, height_cells_ - 1);
    return samples_[static_cast<std::size_t>(iz) * width_cells_ + static_cast<std::size_t>(ix)];
}

BiomeId BiomeMap::sample(double x, double z) const noexcept {
    const double fx = std::clamp(x * inv_cell_size_, 0.0, static_cast<double>(width_cells_ - 1));
    const double fz = std::clamp(z * inv_cell_size_, 0.0, static_cast<double>(height_cells_ - 1));

    const int ix0 = static_cast<int>(std::floor(fx));
    const int iz0 = static_cast<int>(std::floor(fz));
    const int ix1 = std::min(ix0 + 1, width_cells_ - 1);
    const int iz1 = std::min(iz0 + 1, height_cells_ - 1);

    // Nearest-neighbor sampling for discrete enum (biomes don't interpolate)
    const double sx = fx - static_cast<double>(ix0);
    const double sz = fz - static_cast<double>(iz0);
    if (sx < 0.5 && sz < 0.5) {
        return sample_discrete(ix0, iz0);
    } else if (sx >= 0.5 && sz < 0.5) {
        return sample_discrete(ix1, iz0);
    } else if (sx < 0.5 && sz >= 0.5) {
        return sample_discrete(ix0, iz1);
    } else {
        return sample_discrete(ix1, iz1);
    }
}

WaterMap::WaterMap(const WaterConfig& config, const Terrain& terrain)
    : config_(config),
      width_cells_(std::max(config.width_cells, 2)),
      height_cells_(std::max(config.height_cells, 2)),
      cell_size_(std::max(config.cell_size, 1e-3)),
      inv_cell_size_(1.0 / cell_size_),
      water_level_(0.0),
      shore_band_max_(std::max(config.shore_band_max, 0.1)),
      depths_(static_cast<std::size_t>(width_cells_ * height_cells_), 0.0),
      shore_distances_(static_cast<std::size_t>(width_cells_ * height_cells_), 0.0),
      flow_accumulation_(static_cast<std::size_t>(width_cells_ * height_cells_), 0.0) {
    // Step 1: Compute water level from terrain height percentile
    std::vector<double> heights;
    heights.reserve(static_cast<std::size_t>(width_cells_ * height_cells_));
    for (int z = 0; z < height_cells_; ++z) {
        for (int x = 0; x < width_cells_; ++x) {
            const double world_x = static_cast<double>(x) * cell_size_;
            const double world_z = static_cast<double>(z) * cell_size_;
            heights.push_back(terrain.height(world_x, world_z));
        }
    }
    std::sort(heights.begin(), heights.end());
    const std::size_t percentile_idx = static_cast<std::size_t>(
        std::floor(heights.size() * std::clamp(config.water_level_percentile, 0.0, 1.0)));
    water_level_ = heights[percentile_idx];

    // Step 2: Generate lakes from height threshold
    for (int z = 0; z < height_cells_; ++z) {
        for (int x = 0; x < width_cells_; ++x) {
            const double world_x = static_cast<double>(x) * cell_size_;
            const double world_z = static_cast<double>(z) * cell_size_;
            const double terrain_height = terrain.height(world_x, world_z);
            const double depth = std::max(0.0, water_level_ - terrain_height);
            const std::size_t idx = static_cast<std::size_t>(z) * width_cells_ + static_cast<std::size_t>(x);
            depths_[idx] = depth;
        }
    }

    // Step 3: Compute flow accumulation for rivers
    compute_flow_accumulation(terrain);

    // Step 4: Add river channels to depth map
    const double min_accum = config.min_flow_accumulation;
    const double sigma = config.river_width_sigma;
    const double sigma_sq = sigma * sigma;
    const int kernel_radius = static_cast<int>(std::ceil(3.0 * sigma));

    for (int z = 0; z < height_cells_; ++z) {
        for (int x = 0; x < width_cells_; ++x) {
            const std::size_t idx = static_cast<std::size_t>(z) * width_cells_ + static_cast<std::size_t>(x);
            if (flow_accumulation_[idx] >= min_accum) {
                // Apply Gaussian kernel for river widening
                double river_depth = 0.0;
                double weight_sum = 0.0;
                for (int dz = -kernel_radius; dz <= kernel_radius; ++dz) {
                    for (int dx = -kernel_radius; dx <= kernel_radius; ++dx) {
                        const int nx = x + dx;
                        const int nz = z + dz;
                        if (nx < 0 || nx >= width_cells_ || nz < 0 || nz >= height_cells_) {
                            continue;
                        }
                        const double dist_sq = static_cast<double>(dx * dx + dz * dz);
                        const double weight = std::exp(-dist_sq / (2.0 * sigma_sq));
                        const std::size_t nidx = static_cast<std::size_t>(nz) * width_cells_ + static_cast<std::size_t>(nx);
                        if (flow_accumulation_[nidx] >= min_accum) {
                            const double normalized_accum = std::min(1.0, flow_accumulation_[nidx] / (min_accum * 5.0));
                            river_depth += weight * normalized_accum * config.river_depth_scale;
                            weight_sum += weight;
                        }
                    }
                }
                if (weight_sum > kEpsilon) {
                    river_depth /= weight_sum;
                    depths_[idx] = std::max(depths_[idx], river_depth);
                }
            }
        }
    }

    // Step 5: Compute shoreline distances
    compute_shoreline_distances();
}

void WaterMap::compute_flow_accumulation(const Terrain& terrain) {
    // D8 flow direction: find steepest descent neighbor
    std::vector<int> flow_direction(static_cast<std::size_t>(width_cells_ * height_cells_), -1);
    constexpr std::array<std::pair<int, int>, 8> d8_dirs = {
        std::make_pair(-1, -1), std::make_pair(0, -1), std::make_pair(1, -1),
        std::make_pair(-1, 0),                        std::make_pair(1, 0),
        std::make_pair(-1, 1),  std::make_pair(0, 1),  std::make_pair(1, 1)
    };

    for (int z = 0; z < height_cells_; ++z) {
        for (int x = 0; x < width_cells_; ++x) {
            const double world_x = static_cast<double>(x) * cell_size_;
            const double world_z = static_cast<double>(z) * cell_size_;
            const double center_height = terrain.height(world_x, world_z);

            double min_height = center_height;
            int best_dir = -1;
            for (int i = 0; i < 8; ++i) {
                const int nx = x + d8_dirs[i].first;
                const int nz = z + d8_dirs[i].second;
                if (nx < 0 || nx >= width_cells_ || nz < 0 || nz >= height_cells_) {
                    continue;
                }
                const double nworld_x = static_cast<double>(nx) * cell_size_;
                const double nworld_z = static_cast<double>(nz) * cell_size_;
                const double neighbor_height = terrain.height(nworld_x, nworld_z);
                if (neighbor_height < min_height) {
                    min_height = neighbor_height;
                    best_dir = i;
                }
            }
            const std::size_t idx = static_cast<std::size_t>(z) * width_cells_ + static_cast<std::size_t>(x);
            flow_direction[idx] = best_dir;
        }
    }

    // Accumulate flow: each cell contributes to its downstream neighbor
    std::fill(flow_accumulation_.begin(), flow_accumulation_.end(), 1.0);  // Start with 1.0 per cell

    // Multiple passes to propagate accumulation (simple approach)
    for (int pass = 0; pass < 3; ++pass) {
        for (int z = 0; z < height_cells_; ++z) {
            for (int x = 0; x < width_cells_; ++x) {
                const std::size_t idx = static_cast<std::size_t>(z) * width_cells_ + static_cast<std::size_t>(x);
                const int dir = flow_direction[idx];
                if (dir >= 0) {
                    const int nx = x + d8_dirs[dir].first;
                    const int nz = z + d8_dirs[dir].second;
                    if (nx >= 0 && nx < width_cells_ && nz >= 0 && nz < height_cells_) {
                        const std::size_t nidx = static_cast<std::size_t>(nz) * width_cells_ + static_cast<std::size_t>(nx);
                        flow_accumulation_[nidx] += flow_accumulation_[idx] * 0.5;  // Dampen accumulation
                    }
                }
            }
        }
    }
}

void WaterMap::compute_shoreline_distances() {
    // Simple distance transform: mark water cells, then propagate distances
    std::fill(shore_distances_.begin(), shore_distances_.end(), std::numeric_limits<double>::max());

    // Mark water cells as distance 0
    for (int z = 0; z < height_cells_; ++z) {
        for (int x = 0; x < width_cells_; ++x) {
            const std::size_t idx = static_cast<std::size_t>(z) * width_cells_ + static_cast<std::size_t>(x);
            if (depths_[idx] > 0.0) {
                shore_distances_[idx] = 0.0;
            }
        }
    }

    // Propagate distances outward (simple chamfer distance)
    for (int pass = 0; pass < 5; ++pass) {
        for (int z = 1; z < height_cells_ - 1; ++z) {
            for (int x = 1; x < width_cells_ - 1; ++x) {
                const std::size_t idx = static_cast<std::size_t>(z) * width_cells_ + static_cast<std::size_t>(x);
                if (depths_[idx] > 0.0) {
                    continue;  // Skip water cells
                }
                double min_dist = shore_distances_[idx];
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dz == 0) {
                            continue;
                        }
                        const int nx = x + dx;
                        const int nz = z + dz;
                        const std::size_t nidx = static_cast<std::size_t>(nz) * width_cells_ + static_cast<std::size_t>(nx);
                        const double dist = shore_distances_[nidx] + cell_size_ * std::sqrt(static_cast<double>(dx * dx + dz * dz));
                        min_dist = std::min(min_dist, dist);
                    }
                }
                shore_distances_[idx] = min_dist;
            }
        }
    }
}

double WaterMap::depth_discrete(int ix, int iz) const noexcept {
    ix = std::clamp(ix, 0, width_cells_ - 1);
    iz = std::clamp(iz, 0, height_cells_ - 1);
    return depths_[static_cast<std::size_t>(iz) * width_cells_ + static_cast<std::size_t>(ix)];
}

double WaterMap::depth(double x, double z) const noexcept {
    const double fx = std::clamp(x * inv_cell_size_, 0.0, static_cast<double>(width_cells_ - 1));
    const double fz = std::clamp(z * inv_cell_size_, 0.0, static_cast<double>(height_cells_ - 1));

    const int ix0 = static_cast<int>(std::floor(fx));
    const int iz0 = static_cast<int>(std::floor(fz));
    const int ix1 = ix0 + 1;
    const int iz1 = iz0 + 1;

    const double sx = fx - static_cast<double>(ix0);
    const double sz = fz - static_cast<double>(iz0);

    const double d00 = depth_discrete(ix0, iz0);
    const double d10 = depth_discrete(ix1, iz0);
    const double d01 = depth_discrete(ix0, iz1);
    const double d11 = depth_discrete(ix1, iz1);

    const double tx0 = lerp(d00, d10, sx);
    const double tx1 = lerp(d01, d11, sx);
    return lerp(tx0, tx1, sz);
}

double WaterMap::shore_distance_discrete(int ix, int iz) const noexcept {
    ix = std::clamp(ix, 0, width_cells_ - 1);
    iz = std::clamp(iz, 0, height_cells_ - 1);
    return shore_distances_[static_cast<std::size_t>(iz) * width_cells_ + static_cast<std::size_t>(ix)];
}

double WaterMap::shore_distance(double x, double z) const noexcept {
    const double fx = std::clamp(x * inv_cell_size_, 0.0, static_cast<double>(width_cells_ - 1));
    const double fz = std::clamp(z * inv_cell_size_, 0.0, static_cast<double>(height_cells_ - 1));

    const int ix0 = static_cast<int>(std::floor(fx));
    const int iz0 = static_cast<int>(std::floor(fz));
    const int ix1 = ix0 + 1;
    const int iz1 = iz0 + 1;

    const double sx = fx - static_cast<double>(ix0);
    const double sz = fz - static_cast<double>(iz0);

    const double d00 = shore_distance_discrete(ix0, iz0);
    const double d10 = shore_distance_discrete(ix1, iz0);
    const double d01 = shore_distance_discrete(ix0, iz1);
    const double d11 = shore_distance_discrete(ix1, iz1);

    const double tx0 = lerp(d00, d10, sx);
    const double tx1 = lerp(d01, d11, sx);
    return lerp(tx0, tx1, sz);
}

PlantSpeciesRegistry::PlantSpeciesRegistry() {
    // Species 0: Grass (plains/forest terrestrial)
    species_[0] = PlantSpecies{
        .id = 0,
        .growth_rate = 2.5,
        .max_energy = 15.0,
        .radius = 0.5,
        .seed_interval = 18.0,
        .seed_radius = 5.0,
        .establish_prob = 0.7,
        .biome_mask = 0x03,  // Plains (0) and Forest (1)
        .zone_mask = 0x04,   // Terrestrial (bit 2)
        .aquatic_depth_min = 0.5,
        .shoreline_depth_max = 1.2,
        .shoreline_distance_min = 0.0,
        .shoreline_distance_max = 8.0
    };

    // Species 1: Reed (wetland/shoreline)
    species_[1] = PlantSpecies{
        .id = 1,
        .growth_rate = 2.0,
        .max_energy = 18.0,
        .radius = 0.6,
        .seed_interval = 22.0,
        .seed_radius = 6.0,
        .establish_prob = 0.6,
        .biome_mask = 0x04,  // Wetland (2)
        .zone_mask = 0x06,   // Shoreline (bit 1) and Terrestrial (bit 2)
        .aquatic_depth_min = 0.5,
        .shoreline_depth_max = 1.5,
        .shoreline_distance_min = 0.0,
        .shoreline_distance_max = 10.0
    };

    // Species 2: Lily (aquatic)
    species_[2] = PlantSpecies{
        .id = 2,
        .growth_rate = 1.5,
        .max_energy = 25.0,
        .radius = 0.8,
        .seed_interval = 30.0,
        .seed_radius = 8.0,
        .establish_prob = 0.5,
        .biome_mask = 0xFF,  // All biomes
        .zone_mask = 0x01,   // Aquatic (bit 0)
        .aquatic_depth_min = 0.6,
        .shoreline_depth_max = 1.2,
        .shoreline_distance_min = 0.0,
        .shoreline_distance_max = 8.0
    };

    // Species 3: Shrub (forest/plains terrestrial)
    species_[3] = PlantSpecies{
        .id = 3,
        .growth_rate = 1.8,
        .max_energy = 22.0,
        .radius = 0.7,
        .seed_interval = 25.0,
        .seed_radius = 7.0,
        .establish_prob = 0.65,
        .biome_mask = 0x03,  // Plains (0) and Forest (1)
        .zone_mask = 0x04,   // Terrestrial (bit 2)
        .aquatic_depth_min = 0.5,
        .shoreline_depth_max = 1.2,
        .shoreline_distance_min = 0.0,
        .shoreline_distance_max = 8.0
    };

    // Species 4: Alpine Moss (alpine terrestrial)
    species_[4] = PlantSpecies{
        .id = 4,
        .growth_rate = 1.2,
        .max_energy = 12.0,
        .radius = 0.4,
        .seed_interval = 35.0,
        .seed_radius = 4.0,
        .establish_prob = 0.55,
        .biome_mask = 0x08,  // Alpine (3)
        .zone_mask = 0x04,   // Terrestrial (bit 2)
        .aquatic_depth_min = 0.5,
        .shoreline_depth_max = 1.2,
        .shoreline_distance_min = 0.0,
        .shoreline_distance_max = 8.0
    };
}

const PlantSpecies& PlantSpeciesRegistry::get(std::uint8_t id) const noexcept {
    if (id < species_.size()) {
        return species_[id];
    }
    return species_[0];  // Return default (grass) for invalid IDs
}

void EnvironmentStats::reset() noexcept {
    biome_biomass.fill(0.0);
    species_counts.fill(0);
    total_biomass = 0.0;
    soil_mean = 0.0;
    land_fraction = 1.0;
}

WaterZone classify_water_zone(const WaterMap& water_map, double x, double z) noexcept {
    const double depth = water_map.depth(x, z);
    const double shoreline_band = std::max(water_map.shore_band_max(), 0.1);
    if (depth >= shoreline_band) {
        return WaterZone::Aquatic;
    }
    if (depth > 0.0) {
        return WaterZone::Shoreline;
    }
    const double shore_distance = water_map.shore_distance(x, z);
    if (shore_distance <= shoreline_band) {
        return WaterZone::Shoreline;
    }
    return WaterZone::Terrestrial;
}

bool species_allows_location(const PlantSpecies& species,
                             BiomeId biome,
                             WaterZone zone,
                             double depth,
                             double shore_distance) noexcept {
    const std::uint8_t biome_bit = 1u << static_cast<std::uint8_t>(biome);
    if ((species.biome_mask & biome_bit) == 0) {
        return false;
    }

    const std::uint8_t zone_bit = 1u << static_cast<std::uint8_t>(zone);
    if ((species.zone_mask & zone_bit) == 0) {
        return false;
    }

    switch (zone) {
    case WaterZone::Aquatic:
        return depth >= species.aquatic_depth_min;
    case WaterZone::Shoreline:
        if (depth > species.shoreline_depth_max) {
            return false;
        }
        return shore_distance >= species.shoreline_distance_min
               && shore_distance <= species.shoreline_distance_max;
    case WaterZone::Terrestrial:
        return shore_distance >= species.shoreline_distance_min;
    }

    return true;
}

void update_environment_stats(entt::registry& registry) {
    EnvironmentStats* stats_ptr = nullptr;
    if (registry.ctx().contains<EnvironmentStats>()) {
        stats_ptr = &registry.ctx().get<EnvironmentStats>();
    } else {
        stats_ptr = &registry.ctx().emplace<EnvironmentStats>();
    }
    EnvironmentStats& stats = *stats_ptr;
    stats.reset();

    if (auto* soil = registry.ctx().find<SoilGrid>()) {
        stats.soil_mean = soil->mean_nutrient();
    }

    const auto* biome_map = registry.ctx().find<BiomeMap>();
    auto plant_view = registry.view<TransformComponent, PlantComponent>();
    for (auto entity : plant_view) {
        const auto& transform = plant_view.get<TransformComponent>(entity);
        const auto& plant = plant_view.get<PlantComponent>(entity);
        if (!plant.alive) {
            continue;
        }

        stats.total_biomass += plant.energy;
        if (plant.species_id < stats.species_counts.size()) {
            stats.species_counts[plant.species_id] += 1;
        }

        int biome_idx = 0;
        if (biome_map != nullptr) {
            biome_idx = std::clamp(static_cast<int>(biome_map->sample(transform.position.x,
                                                                      transform.position.z)),
                                   0,
                                   static_cast<int>(stats.biome_biomass.size()) - 1);
        }
        stats.biome_biomass[biome_idx] += plant.energy;
    }

    if (const auto* water_map = registry.ctx().find<WaterMap>()) {
        int land_cells = 0;
        int water_cells = 0;
        const double width = static_cast<double>(water_map->width()) * water_map->cell_size();
        const double height = static_cast<double>(water_map->height()) * water_map->cell_size();
        const int samples = 32;
        const double step_x = std::max(width / samples, water_map->cell_size());
        const double step_z = std::max(height / samples, water_map->cell_size());

        for (double x = 0.0; x < width; x += step_x) {
            for (double z = 0.0; z < height; z += step_z) {
                if (water_map->is_water(x, z)) {
                    ++water_cells;
                } else {
                    ++land_cells;
                }
            }
        }

        const double total = static_cast<double>(land_cells + water_cells);
        if (total > 0.0) {
            stats.land_fraction = static_cast<double>(land_cells) / total;
        }
    }
}

}  // namespace evolution::sim
