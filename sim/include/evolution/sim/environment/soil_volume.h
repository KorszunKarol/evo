#pragma once

#include <cstddef>
#include <vector>

#include "evolution/sim/math/fixed_point.h"
#include "evolution/sim/math_types.h"

// Forward declaration
namespace evolution::sim {
class BiomeMap;
}

namespace evolution::sim {

///
/// @brief Represents the chemical state of a single soil voxel.
///
struct SoilVoxel {
    math::Fixed64 nitrogen{0};
    math::Fixed64 phosphorus{0};
    math::Fixed64 potassium{0};
    math::Fixed64 ph{7};
    math::Fixed64 water{0};
};

///
/// @brief Configuration for the 3D soil volume.
///
struct SoilVolumeConfig {
    int width{64};   ///< X axis
    int height{16};  ///< Yush axis (vertical depth)
    int depth{64};   ///< Z axis
    double voxel_size{1.0};
    math::Fixed64 diffusion_rate{0.1};
};

///
/// @brief 3D grid managing soil nutrients and chemistry.
///
/// @details Uses a 3D buffer to store NPK, pH, and water values.
///          Supports 3D diffusion and trilinear sampling.
///
class SoilVolume {
public:
    explicit SoilVolume(const SoilVolumeConfig& config);

    /// @brief Access voxel by grid coordinates.
    [[nodiscard]] SoilVoxel& at(int x, int y, int z);
    [[nodiscard]] const SoilVoxel& at(int x, int y, int z) const;

    /// @brief Sample soil properties at a world position using trilinear interpolation.
    [[nodiscard]] SoilVoxel sample(const Vec3& pos) const;

    /// @brief Perform one step of diffusion on all nutrients.
    /// @param dt Time step in seconds.
    void diffuse(math::Fixed64 dt);

    /// @brief Regenerate nutrients based on biomes.
    void regenerate(math::Fixed64 dt, 
                    const BiomeMap* biome_map, 
                    double climate_mult);

    [[nodiscard]] int width() const { return config_.width; }
    [[nodiscard]] int height() const { return config_.height; }
    [[nodiscard]] int depth() const { return config_.depth; }
    [[nodiscard]] double voxel_size() const { return config_.voxel_size; }

private:
    [[nodiscard]] std::size_t index(int x, int y, int z) const;

    SoilVolumeConfig config_;
    std::vector<SoilVoxel> voxels_;
    std::vector<SoilVoxel> scratch_;  ///< Double buffer for diffusion
};

}  // namespace evolution::sim
