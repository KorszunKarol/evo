#include "evolution/sim/physics/broad_phase.h"

#include <algorithm>
#include <cmath>

namespace evolution::sim {

namespace {

[[nodiscard]] CellKey make_cell_key(int ix, int iy, int iz) noexcept {
    return CellKey{ix, iy, iz};
}

[[nodiscard]] std::pair<int, int> range_for_axis(double min_value, double max_value, double inv_cell) noexcept {
    const int min_cell = static_cast<int>(std::floor(min_value * inv_cell));
    const int max_cell = static_cast<int>(std::floor(max_value * inv_cell));
    return {min_cell, max_cell};
}

[[nodiscard]] bool is_valid_pair(entt::entity a, entt::entity b) noexcept {
    return a != b && a != entt::null && b != entt::null;
}

}  // namespace

SpatialHash::SpatialHash(double cell_size) noexcept
    : cell_size_(std::max(0.1, cell_size)), inv_cell_size_(1.0 / std::max(0.1, cell_size)) {}

void SpatialHash::clear() {
    for (auto& [key, occupants] : cells_) {
        occupants.clear();
    }
}

void SpatialHash::insert(entt::entity entity, const WorldAabb& bounds) {
    const auto [min_x, max_x] = range_for_axis(bounds.min.x, bounds.max.x, inv_cell_size_);
    const auto [min_y, max_y] = range_for_axis(bounds.min.y, bounds.max.y, inv_cell_size_);
    const auto [min_z, max_z] = range_for_axis(bounds.min.z, bounds.max.z, inv_cell_size_);

    for (int ix = min_x; ix <= max_x; ++ix) {
        for (int iy = min_y; iy <= max_y; ++iy) {
            for (int iz = min_z; iz <= max_z; ++iz) {
                cells_[make_cell_key(ix, iy, iz)].push_back(entity);
            }
        }
    }
}

void SpatialHash::finalize() {
    for (auto& [key, occupants] : cells_) {
        std::sort(occupants.begin(), occupants.end(), [](entt::entity lhs, entt::entity rhs) {
            return entt::to_integral(lhs) < entt::to_integral(rhs);
        });
        occupants.erase(std::unique(occupants.begin(), occupants.end()), occupants.end());
    }
}

void SpatialHash::build_candidate_pairs(std::vector<std::pair<entt::entity, entt::entity>>& out_pairs) const {
    for (const auto& [key, occupants] : cells_) {
        const std::size_t count = occupants.size();
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = i + 1; j < count; ++j) {
                const entt::entity a = occupants[i];
                const entt::entity b = occupants[j];
                if (!is_valid_pair(a, b)) {
                    continue;
                }
                if (entt::to_integral(a) < entt::to_integral(b)) {
                    out_pairs.emplace_back(a, b);
                } else {
                    out_pairs.emplace_back(b, a);
                }
            }
        }
    }

    std::sort(out_pairs.begin(), out_pairs.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.first == rhs.first) {
            return entt::to_integral(lhs.second) < entt::to_integral(rhs.second);
        }
        return entt::to_integral(lhs.first) < entt::to_integral(rhs.first);
    });
    out_pairs.erase(std::unique(out_pairs.begin(), out_pairs.end()), out_pairs.end());
}

WorldAabb compute_world_aabb(const ColliderComponent& collider, const Vec3& position) {
    const Vec3 center = position + collider.offset;
    switch (collider.type) {
        case ShapeType::Sphere: {
            const double r = collider.sphere.radius;
            return WorldAabb{center - Vec3{r, r, r}, center + Vec3{r, r, r}};
        }
        case ShapeType::Aabb: {
            const Vec3 he = collider.aabb.half_extents;
            return WorldAabb{center - he, center + he};
        }
        case ShapeType::CapsuleY: {
            const double r = collider.capsule.radius;
            const double h = collider.capsule.half_height;
            const Vec3 extents{r, h + r, r};
            return WorldAabb{center - extents, center + extents};
        }
        default:
            return WorldAabb{center, center};
    }
}

}  // namespace evolution::sim


