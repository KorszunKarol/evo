#pragma once

#include <cstddef>
#include <vector>

#include "evolution/sim/math_types.h"

// Forward declaration
namespace evolution::sim {
class BiomeMap;
}

namespace evolution::sim {

/**
 * @brief Represents the chemical state of a single soil voxel.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1).
 * @note Units are simulation-defined; nutrient magnitudes are not normalized.
 * @warning Values are mutable and not range-clamped by this struct.
 * @threadsafe @notthreadsafe Access requires external synchronization.
 */
struct SoilVoxel {
    float nitrogen{0.0F};
    float phosphorus{0.0F};
    float potassium{0.0F};
    float ph{7.0F};
    float water{0.0F};
};

/**
 * @brief Configuration for the 3D soil volume.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1).
 * @note Dimensions are expressed in voxel counts; voxel_size is in world meters.
 * @warning Width/height/depth must be positive to avoid invalid indexing.
 * @threadsafe @notthreadsafe Treat as immutable after construction.
 */
struct SoilVolumeConfig {
    int width{64};   ///< X axis
    int height{16};  ///< Y axis (vertical depth)
    int depth{64};   ///< Z axis
    double voxel_size{1.0};
    float diffusion_rate{0.1F};
};

/**
 * @brief 3D grid managing soil nutrients and chemistry.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(W*H*D) storage.
 * @note Uses a 3D buffer to store NPK, pH, and water values with trilinear sampling.
 * @warning Callers must ensure indices and world positions are within configured bounds.
 * @threadsafe @notthreadsafe Mutations must be single-threaded.
 */
class SoilVolume {
public:
    /**
     * @brief Construct a soil volume with configured dimensions.
     * @param config Volume dimensions and diffusion parameters.
     * @return None.
     * @throws None.
     * @complexity O(W*H*D) to allocate voxel buffers.
     * @note Initializes all voxel values to defaults.
     * @warning Large dimensions can allocate significant memory.
     * @threadsafe @notthreadsafe Construction is not thread-safe.
     */
    explicit SoilVolume(const SoilVolumeConfig& config);

    /**
     * @brief Access voxel by grid coordinates.
     * @param x X index in [0, width).
     * @param y Y index in [0, height).
     * @param z Z index in [0, depth).
     * @return Mutable reference to the voxel.
     * @throws None.
     * @complexity O(1).
     * @note No bounds checks are performed.
     * @warning Passing out-of-range indices is undefined behavior.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] SoilVoxel& at(int x, int y, int z);
    /**
     * @brief Access voxel by grid coordinates (const).
     * @param x X index in [0, width).
     * @param y Y index in [0, height).
     * @param z Z index in [0, depth).
     * @return Const reference to the voxel.
     * @throws None.
     * @complexity O(1).
     * @note No bounds checks are performed.
     * @warning Passing out-of-range indices is undefined behavior.
     * @threadsafe @notthreadsafe Safe only with no concurrent writes.
     */
    [[nodiscard]] const SoilVoxel& at(int x, int y, int z) const;

    /**
     * @brief Sample soil properties at a world position using trilinear interpolation.
     * @param pos World position in meters.
     * @return Interpolated SoilVoxel values at the sampled location.
     * @throws None.
     * @complexity O(1).
     * @note Positions are clamped to the volume bounds.
     * @warning Sampling results are undefined if voxel_size <= 0.
     * @threadsafe @notthreadsafe Safe only with no concurrent writes.
     */
    [[nodiscard]] SoilVoxel sample(const Vec3& pos) const;

    /**
     * @brief Perform one step of diffusion on all nutrients.
     * @param dt Time step in seconds.
     * @return None.
     * @throws None.
     * @complexity O(W*H*D).
     * @note Uses a scratch buffer to avoid read/write conflicts.
     * @warning Boundary voxels are not updated; dt <= 0 is a no-op.
     * @threadsafe @notthreadsafe Mutates internal voxel buffers.
     */
    void diffuse(double dt);

    /**
     * @brief Regenerate nutrients based on biomes.
     * @param dt Time step in seconds.
     * @param biome_map Optional biome map for per-biome rates (nullable).
     * @param climate_mult Climate multiplier applied to regeneration rates.
     * @return None.
     * @throws None.
     * @complexity O(W*D*H).
     * @note Regeneration is applied to the nitrogen channel only.
     * @warning biome_map may be null; callers must pass a valid pointer or nullptr.
     * @threadsafe @notthreadsafe Mutates internal voxel buffers.
     */
    void regenerate(double dt, const BiomeMap* biome_map, double climate_mult);

    /**
     * @brief Retrieve the voxel grid width.
     * @param None.
     * @return Width in voxel cells.
     * @throws None.
     * @complexity O(1).
     * @note Excludes any padding.
     * @warning None.
     * @threadsafe @threadsafe Read-only accessor.
     */
    [[nodiscard]] int width() const { return config_.width; }
    /**
     * @brief Retrieve the voxel grid height.
     * @param None.
     * @return Height in voxel cells.
     * @throws None.
     * @complexity O(1).
     * @note Represents vertical depth.
     * @warning None.
     * @threadsafe @threadsafe Read-only accessor.
     */
    [[nodiscard]] int height() const { return config_.height; }
    /**
     * @brief Retrieve the voxel grid depth.
     * @param None.
     * @return Depth in voxel cells.
     * @throws None.
     * @complexity O(1).
     * @note Depth corresponds to world-space Z.
     * @warning None.
     * @threadsafe @threadsafe Read-only accessor.
     */
    [[nodiscard]] int depth() const { return config_.depth; }
    /**
     * @brief Retrieve the voxel edge length in meters.
     * @param None.
     * @return Voxel size in world meters.
     * @throws None.
     * @complexity O(1).
     * @note Used to convert world positions to grid indices.
     * @warning None.
     * @threadsafe @threadsafe Read-only accessor.
     */
    [[nodiscard]] double voxel_size() const { return config_.voxel_size; }

private:
    /**
     * @brief Compute flat buffer index for a voxel coordinate.
     * @param x X index in [0, width).
     * @param y Y index in [0, height).
     * @param z Z index in [0, depth).
     * @return Flat array index into voxel buffers.
     * @throws None.
     * @complexity O(1).
     * @note Does not validate bounds.
     * @warning Passing out-of-range indices is undefined behavior.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::size_t index(int x, int y, int z) const;

    SoilVolumeConfig config_;
    std::vector<SoilVoxel> voxels_;
    std::vector<SoilVoxel> scratch_;  ///< Double buffer for diffusion
};

}  // namespace evolution::sim
