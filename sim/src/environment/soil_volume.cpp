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
    double gy = pos.y / config_.voxel_size; // Assuming Y down? or Y up? Terrain is usually Y up. 
    // If soil volume starts at some Y, we need offset. Assuming Y=0 is top/bottom.
    // Since this is a volume, let's assume Y=0 is the surface (0 index) or bottom (0 index).
    // Given Terrain uses Y for height, and soil is usually *below* terrain or *is* terrain.
    // For now, simple mapping: Y index = pos.y / size.
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

    auto lerp_fixed = [](math::Fixed64 a, math::Fixed64 b, double t) {
        return math::Fixed64(a.to_double() + (b.to_double() - a.to_double()) * t);
    };

    auto lerp_voxel = [&](const SoilVoxel& v0, const SoilVoxel& v1, double t) {
        SoilVoxel res;
        res.nitrogen = lerp_fixed(v0.nitrogen, v1.nitrogen, t);
        res.phosphorus = lerp_fixed(v0.phosphorus, v1.phosphorus, t);
        res.potassium = lerp_fixed(v0.potassium, v1.potassium, t);
        res.ph = lerp_fixed(v0.ph, v1.ph, t);
        res.water = lerp_fixed(v0.water, v1.water, t);
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

void SoilVolume::diffuse(math::Fixed64 dt) {
    const math::Fixed64 rate = config_.diffusion_rate * dt;
    if (rate.raw() <= 0) return;

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

                auto diffuse_comp = [&](math::Fixed64 val, 
                                      math::Fixed64 vxm, math::Fixed64 vxp,
                                      math::Fixed64 vym, math::Fixed64 vyp,
                                      math::Fixed64 vzm, math::Fixed64 vzp) {
                    math::Fixed64 sum = vxm + vxp + vym + vyp + vzm + vzp;
                    math::Fixed64 laplacian = sum - (val * math::Fixed64(6));
                    return val + rate * laplacian;
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

void SoilVolume::regenerate(math::Fixed64 dt, const BiomeMap* biome_map, double climate_mult) {
    // Hardcoded baselines for now, similar to SoilGrid
    constexpr std::array<double, 4> baselines = {4.0, 5.0, 6.0, 3.0}; 
    constexpr std::array<double, 4> rates = {0.06, 0.08, 0.10, 0.04};
    
    // Default fallback
    const math::Fixed64 def_rate = math::Fixed64(0.05 * climate_mult);
    const math::Fixed64 def_base = math::Fixed64(4.0);
    const math::Fixed64 dt_fix = dt;

    for (int z = 0; z < config_.depth; ++z) {
        for (int x = 0; x < config_.width; ++x) {
            math::Fixed64 rate = def_rate;
            math::Fixed64 base = def_base;

            if (biome_map) {
                double wx = static_cast<double>(x) * config_.voxel_size;
                double wz = static_cast<double>(z) * config_.voxel_size;
                BiomeId biome = biome_map->sample(wx, wz);
                int idx = static_cast<int>(biome);
                if (idx >= 0 && idx < 4) {
                    rate = math::Fixed64(rates[idx] * climate_mult);
                    base = math::Fixed64(baselines[idx]);
                }
            }
            
            // Apply to full column (y)
            math::Fixed64 step = rate * dt_fix;
            for (int y = 0; y < config_.height; ++y) {
                SoilVoxel& v = at(x, y, z);
                // Only regenerate nitrogen for now as proxy for general nutrients
                if (v.nitrogen < base) {
                    v.nitrogen = v.nitrogen + step;
                    if (v.nitrogen > base) v.nitrogen = base;
                }
            }
        }
    }
}

}  // namespace evolution::sim
