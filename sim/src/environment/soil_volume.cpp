#include "evolution/sim/environment/soil_volume.h"
#include "evolution/sim/environment/environment.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace evolution::sim {

SoilVolume::SoilVolume(const SoilVolumeConfig& config)
    : config_(config),
      voxels_(config.width * config.height * config.depth),
      scratch_(config.width * config.height * config.depth) {}

std::size_t SoilVolume::index(int x, int y, int z) const {
    return static_cast<std::size_t>(
        (y * config_.depth + z) * config_.width + x);
}

SoilVoxel& SoilVolume::at(int x, int y, int z) {
    return voxels_[index(x, y, z)];
}

const SoilVoxel& SoilVolume::at(int x, int y, int z) const {
    return voxels_[index(x, y, z)];
}

SoilVoxel SoilVolume::sample(const Vec3& pos) const {
    double gx = pos.x / config_.voxel_size;
    double gy = pos.y / config_.voxel_size;
    double gz = pos.z / config_.voxel_size;

    gx = std::clamp(gx, 0.0, static_cast<double>(config_.width) - 1.0001);
    gy = std::clamp(gy, 0.0, static_cast<double>(config_.height) - 1.0001);
    gz = std::clamp(gz, 0.0, static_cast<double>(config_.depth) - 1.0001);

    const int x0 = static_cast<int>(std::floor(gx));
    const int y0 = static_cast<int>(std::floor(gy));
    const int z0 = static_cast<int>(std::floor(gz));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const int z1 = z0 + 1;

    const double tx = gx - x0;
    const double ty = gy - y0;
    const double tz = gz - z0;

    auto lerp_float = [](float a, float b, double t) {
        return static_cast<float>(a + static_cast<float>((b - a) * t));
    };

    auto lerp_voxel = [&](const SoilVoxel& v0, const SoilVoxel& v1, double t) {
        SoilVoxel res;
        res.nitrogen = lerp_float(v0.nitrogen, v1.nitrogen, t);
        res.phosphorus = lerp_float(v0.phosphorus, v1.phosphorus, t);
        res.potassium = lerp_float(v0.potassium, v1.potassium, t);
        res.ph = lerp_float(v0.ph, v1.ph, t);
        res.water = lerp_float(v0.water, v1.water, t);
        return res;
    };

    const SoilVoxel& c000 = at(x0, y0, z0);
    const SoilVoxel& c100 = at(x1, y0, z0);
    const SoilVoxel& c010 = at(x0, y1, z0);
    const SoilVoxel& c110 = at(x1, y1, z0);
    const SoilVoxel& c001 = at(x0, y0, z1);
    const SoilVoxel& c101 = at(x1, y0, z1);
    const SoilVoxel& c011 = at(x0, y1, z1);
    const SoilVoxel& c111 = at(x1, y1, z1);

    SoilVoxel c00 = lerp_voxel(c000, c100, tx);
    SoilVoxel c01 = lerp_voxel(c001, c101, tx);
    SoilVoxel c10 = lerp_voxel(c010, c110, tx);
    SoilVoxel c11 = lerp_voxel(c011, c111, tx);

    SoilVoxel c0 = lerp_voxel(c00, c10, ty);
    SoilVoxel c1 = lerp_voxel(c01, c11, ty);

    return lerp_voxel(c0, c1, tz);
}

void SoilVolume::diffuse(double dt) {
    const double rate = static_cast<double>(config_.diffusion_rate) * dt;
    if (rate <= 0.0) {
        return;
    }

    for (int y = 1; y < config_.height - 1; ++y) {
        for (int z = 1; z < config_.depth - 1; ++z) {
            for (int x = 1; x < config_.width - 1; ++x) {
                const std::size_t idx = index(x, y, z);
                const SoilVoxel& center = voxels_[idx];
                
                const SoilVoxel& xm = voxels_[index(x - 1, y, z)];
                const SoilVoxel& xp = voxels_[index(x + 1, y, z)];
                const SoilVoxel& ym = voxels_[index(x, y - 1, z)];
                const SoilVoxel& yp = voxels_[index(x, y + 1, z)];
                const SoilVoxel& zm = voxels_[index(x, y, z - 1)];
                const SoilVoxel& zp = voxels_[index(x, y, z + 1)];

                auto diffuse_comp = [&](float val,
                                        float vxm, float vxp,
                                        float vym, float vyp,
                                        float vzm, float vzp) {
                    const float sum = vxm + vxp + vym + vyp + vzm + vzp;
                    const float laplacian = sum - (val * 6.0F);
                    return static_cast<float>(val + rate * static_cast<double>(laplacian));
                };

                SoilVoxel& next = scratch_[idx];
                next.nitrogen = diffuse_comp(center.nitrogen, xm.nitrogen, xp.nitrogen, ym.nitrogen, yp.nitrogen, zm.nitrogen, zp.nitrogen);
                next.phosphorus = diffuse_comp(center.phosphorus, xm.phosphorus, xp.phosphorus, ym.phosphorus, yp.phosphorus, zm.phosphorus, zp.phosphorus);
                next.potassium = diffuse_comp(center.potassium, xm.potassium, xp.potassium, ym.potassium, yp.potassium, zm.potassium, zp.potassium);
                next.ph = diffuse_comp(center.ph, xm.ph, xp.ph, ym.ph, yp.ph, zm.ph, zp.ph);
                next.water = diffuse_comp(center.water, xm.water, xp.water, ym.water, yp.water, zm.water, zp.water);
            }
        }
    }
    
    for (int y = 1; y < config_.height - 1; ++y) {
        for (int z = 1; z < config_.depth - 1; ++z) {
            for (int x = 1; x < config_.width - 1; ++x) {
                std::size_t idx = index(x, y, z);
                voxels_[idx] = scratch_[idx];
            }
        }
    }
}

void SoilVolume::regenerate(double dt, const BiomeMap* biome_map, double climate_mult) {
    // Hardcoded baselines for now, similar to SoilGrid
    constexpr std::array<double, 4> baselines = {4.0, 5.0, 6.0, 3.0}; 
    constexpr std::array<double, 4> rates = {0.06, 0.08, 0.10, 0.04};
    
    // Default fallback
    const double def_rate = 0.05 * climate_mult;
    const float def_base = 4.0F;

    for (int z = 0; z < config_.depth; ++z) {
        for (int x = 0; x < config_.width; ++x) {
            double rate = def_rate;
            float base = def_base;

            if (biome_map) {
                double wx = static_cast<double>(x) * config_.voxel_size;
                double wz = static_cast<double>(z) * config_.voxel_size;
                BiomeId biome = biome_map->sample(wx, wz);
                int idx = static_cast<int>(biome);
                if (idx >= 0 && idx < 4) {
                    rate = rates[idx] * climate_mult;
                    base = static_cast<float>(baselines[idx]);
                }
            }
            
            // Apply to full column (y)
            const float step = static_cast<float>(rate * dt);
            for (int y = 0; y < config_.height; ++y) {
                SoilVoxel& v = at(x, y, z);
                // Only regenerate nitrogen for now as proxy for general nutrients
                if (v.nitrogen < base) {
                    v.nitrogen += step;
                    if (v.nitrogen > base) {
                        v.nitrogen = base;
                    }
                }
            }
        }
    }
}

}  // namespace evolution::sim
