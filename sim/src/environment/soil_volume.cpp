#include "evolution/sim/environment/soil_volume.h"

#include <algorithm>
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
    // Transform to grid coordinates
    // Assuming Y=0 is top of soil for now, or bottom? 
    // Let's assume standard grid mapping: pos / voxel_size
    double gx = pos.x / config_.voxel_size;
    double gy = pos.y / config_.voxel_size;
    double gz = pos.z / config_.voxel_size;

    // Clamp to valid range (minus 1 for interpolation)
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

    // Helper for linear interpolation of Fixed64
    auto lerp_fixed = [](math::Fixed64 a, math::Fixed64 b, double t) {
        // Convert to double for interpolation to avoid precision headaches with Fixed64 mult logic for now, 
        // or assume t is Fixed64? 't' is derived from double world pos.
        // For strictly deterministic sampling, 'pos' should ideally be fixed point too.
        // But Vec3 is double. We'll convert to double, lerp, back to fixed.
        return math::Fixed64(a.to_double() + (b.to_double() - a.to_double()) * t);
    };

    // Helper to interpolation whole voxel
    auto lerp_voxel = [&](const SoilVoxel& v0, const SoilVoxel& v1, double t) {
        SoilVoxel res;
        res.nitrogen = lerp_fixed(v0.nitrogen, v1.nitrogen, t);
        res.phosphorus = lerp_fixed(v0.phosphorus, v1.phosphorus, t);
        res.potassium = lerp_fixed(v0.potassium, v1.potassium, t);
        res.ph = lerp_fixed(v0.ph, v1.ph, t);
        res.water = lerp_fixed(v0.water, v1.water, t);
        return res;
    };

    // Trilinear interpolation
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
    // 3D Laplacian diffusion (7-point stencil)
    // k * dt
    const math::Fixed64 rate = config_.diffusion_rate * dt;
    
    // If rate is negligible, skip
    if (rate.raw() <= 0) return;

    for (int y = 1; y < config_.height - 1; ++y) {
        for (int z = 1; z < config_.depth - 1; ++z) {
            for (int x = 1; x < config_.width - 1; ++x) {
                const std::size_t idx = index(x, y, z);
                const SoilVoxel& center = voxels_[idx];
                
                // Sum neighbors
                // x-axis
                const SoilVoxel& xm = voxels_[index(x - 1, y, z)];
                const SoilVoxel& xp = voxels_[index(x + 1, y, z)];
                // y-axis
                const SoilVoxel& ym = voxels_[index(x, y - 1, z)];
                const SoilVoxel& yp = voxels_[index(x, y + 1, z)];
                // z-axis
                const SoilVoxel& zm = voxels_[index(x, y, z - 1)];
                const SoilVoxel& zp = voxels_[index(x, y, z + 1)];

                // Apply diffusion for each component
                // NewVal = Val + Rate * (SumNeighbor - 6*Val)
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
    
    // Handle boundaries (copy or reflect? For now, just keep static or do nothing to them)
    // Ideally we iterate inner and ignore boundaries, effectively fixed boundary conditions.
    // Or we copy scratch to voxels but only the inner part?
    // Let's copy everything, assuming boundaries were initialized in scratch or don't change.
    // To be safe, we should copy boundaries from 'voxels_' to 'scratch_' before swap?
    // Or just iterate boundaries separately. 
    // Simplest: Leave boundaries constant (Dirichlet).
    
    // Copy inner scratch back to voxels.
    // Actually, just swapping is faster, but then scratch has old data on boundaries.
    // If we initialize scratch once, then swap, the boundaries of 'voxels' (now old scratch) might be garbage.
    // Correct approach: compute full grid or handle boundaries properly.
    // Let's do simple copy for now to be correct.
    for (int y = 1; y < config_.height - 1; ++y) {
        for (int z = 1; z < config_.depth - 1; ++z) {
            for (int x = 1; x < config_.width - 1; ++x) {
                std::size_t idx = index(x, y, z);
                voxels_[idx] = scratch_[idx];
            }
        }
    }
}

}  // namespace evolution::sim

