#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <utility>

#include <entt/entt.hpp>

#include "evolution/sim/components.h"
#include "evolution/sim/math_types.h"
#include "evolution/sim/physics/physics_types.h"

namespace evolution::sim {

/**
 * @brief Axis-aligned bounding box expressed with minimum and maximum corners.
 */
struct WorldAabb {
    Vec3 min{0.0, 0.0, 0.0};  ///< Lower corner coordinates.
    Vec3 max{0.0, 0.0, 0.0};  ///< Upper corner coordinates.
};

/**
 * @brief Spatial hash key representing a grid cell in integer coordinates.
 */
struct CellKey {
    int ix{0};  ///< Cell coordinate along X.
    int iy{0};  ///< Cell coordinate along Y.
    int iz{0};  ///< Cell coordinate along Z.

    /**
     * @brief Comparison operator used for deterministic ordering.
     *
     * @param other const CellKey& Key to compare against.
     * @return bool True when the keys are equal.
     */
    [[nodiscard]] bool operator==(const CellKey& other) const noexcept {
        return ix == other.ix && iy == other.iy && iz == other.iz;
    }

    /**
     * @brief Lexicographic order for sorting keys deterministically.
     *
     * @param other const CellKey& Key to compare against.
     * @return bool True when this key is lexicographically smaller.
     */
    [[nodiscard]] bool operator<(const CellKey& other) const noexcept {
        if (ix != other.ix) {
            return ix < other.ix;
        }
        if (iy != other.iy) {
            return iy < other.iy;
        }
        return iz < other.iz;
    }
};

/**
 * @brief Hash functor for CellKey suitable for unordered containers.
 */
struct CellKeyHasher {
    /**
     * @brief Computes a 64-bit hash value for the supplied key.
     *
     * @param key const CellKey& Key to hash.
     * @return std::size_t Hashed value.
     */
    [[nodiscard]] std::size_t operator()(const CellKey& key) const noexcept {
        const std::uint64_t x = static_cast<std::uint64_t>(key.ix) * 73856093u;
        const std::uint64_t y = static_cast<std::uint64_t>(key.iy) * 19349663u;
        const std::uint64_t z = static_cast<std::uint64_t>(key.iz) * 83492791u;
        return static_cast<std::size_t>(x ^ y ^ z);
    }
};

/**
 * @brief Stable spatial hash that produces deterministic broad-phase pairs.
 */
class SpatialHash {
public:
    /**
     * @brief Constructs the spatial hash with the given cell size.
     *
     * @param cell_size double Edge length of a grid cell in meters.
     * @note A larger cell size reduces hashing cost but increases candidate pairs.
     */
    explicit SpatialHash(double cell_size = 1.0) noexcept;

    /**
     * @brief Clears all stored entries.
     *
     * @complexity O(N) due to container destruction.
     */
    void clear();

    /**
     * @brief Inserts an entity using its world-space AABB.
     *
     * @param entity entt::entity Entity identifier.
     * @param bounds const WorldAabb& Axis-aligned bounds in world space.
     * @complexity O(K) where K is number of cells overlapped by the bounds.
     */
    void insert(entt::entity entity, const WorldAabb& bounds);

    /**
     * @brief Finalizes cell storage by sorting occupants for determinism.
     */
    void finalize();

    /**
     * @brief Generates candidate body pairs using the populated spatial hash.
     *
     * @param[out] out_pairs std::vector<std::pair<entt::entity, entt::entity>>& Output buffer to fill.
     * @complexity O(M log M) where M is candidate pair count due to sorting and deduplication.
     * @warning The vector is not cleared by the function; caller should clear beforehand.
     */
    void build_candidate_pairs(std::vector<std::pair<entt::entity, entt::entity>>& out_pairs) const;

private:
    double cell_size_{1.0};
    double inv_cell_size_{1.0};
    std::unordered_map<CellKey, std::vector<entt::entity>, CellKeyHasher> cells_;
};

/**
 * @brief Computes the world-space AABB for a collider located at position with offset.
 *
 * @param collider const ColliderComponent& Collider definition.
 * @param position const Vec3& Entity world position.
 * @return WorldAabb Bounding box encapsulating the collider.
 */
[[nodiscard]] WorldAabb compute_world_aabb(const ColliderComponent& collider, const Vec3& position);

}  // namespace evolution::sim


