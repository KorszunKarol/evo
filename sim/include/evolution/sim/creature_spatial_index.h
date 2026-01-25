#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>
#include "evolution/sim/components.h"
#include "evolution/sim/math_types.h"

namespace evolution::sim {

/**
 * @brief Spatial acceleration structure for creature queries.
 * 
 * Tracks entities with MetabolismComponent for efficient predator-prey lookups.
 * Uses a simple 2D grid hashing.
 */
class CreatureSpatialIndex {
public:
    explicit CreatureSpatialIndex(double cell_size = 5.0) : cell_size_(cell_size) {}
    
    void rebuild(entt::registry& registry) {
        clear();
        
        // 1. Index Alive Creatures
        auto alive_view = registry.view<TransformComponent, MetabolismComponent, DietComponent>();
        for (auto entity : alive_view) {
            const auto& transform = alive_view.get<TransformComponent>(entity);
            const auto& metabolism = alive_view.get<MetabolismComponent>(entity);
            
            if (metabolism.energy <= 0.0) continue; // Skip dead
            
            const std::int64_t cell_x = static_cast<std::int64_t>(std::floor(transform.position.x / cell_size_));
            const std::int64_t cell_z = static_cast<std::int64_t>(std::floor(transform.position.z / cell_size_));
            const std::int64_t key = (cell_x * 73856093) ^ (cell_z * 19349663);
            
            cells_[key].push_back(entity);
        }

        // 2. Index Corpses
        auto corpse_view = registry.view<TransformComponent, CorpseComponent>();
        for (auto entity : corpse_view) {
            const auto& transform = corpse_view.get<TransformComponent>(entity);
            const auto& corpse = corpse_view.get<CorpseComponent>(entity);
            
            if (!corpse.edible || corpse.biomass <= 0.0) continue;
            
            const std::int64_t cell_x = static_cast<std::int64_t>(std::floor(transform.position.x / cell_size_));
            const std::int64_t cell_z = static_cast<std::int64_t>(std::floor(transform.position.z / cell_size_));
            const std::int64_t key = (cell_x * 73856093) ^ (cell_z * 19349663);
            
            cells_[key].push_back(entity);
        }
    }
    
    void clear() {
        cells_.clear();
    }
    
    template<typename Callback>
    void for_each_in_radius(entt::registry& registry,
                            const Vec3& center,
                            double radius,
                            Callback&& callback) const {
        const double r = radius;
        const std::int64_t min_x = static_cast<std::int64_t>(std::floor((center.x - r) / cell_size_));
        const std::int64_t max_x = static_cast<std::int64_t>(std::floor((center.x + r) / cell_size_));
        const std::int64_t min_z = static_cast<std::int64_t>(std::floor((center.z - r) / cell_size_));
        const std::int64_t max_z = static_cast<std::int64_t>(std::floor((center.z + r) / cell_size_));

        const double radius_sq = radius * radius;

        for (std::int64_t z = min_z; z <= max_z; ++z) {
            for (std::int64_t x = min_x; x <= max_x; ++x) {
                const std::int64_t key = (x * 73856093) ^ (z * 19349663);
                auto it = cells_.find(key);
                if (it == cells_.end()) continue;

                for (auto entity : it->second) {
                    const auto& transform = registry.get<TransformComponent>(entity);
                    const double dx = transform.position.x - center.x;
                    const double dz = transform.position.z - center.z;
                    if (dx*dx + dz*dz <= radius_sq) {
                        if (!callback(entity)) return;
                    }
                }
            }
        }
    }
    
    [[nodiscard]] std::optional<entt::entity> find_nearest(
        entt::registry& registry,
        const Vec3& center,
        double max_radius,
        DietType target_diet) const {
            
        std::optional<entt::entity> nearest = std::nullopt;
        double min_dist_sq = max_radius * max_radius;
        
        for_each_in_radius(registry, center, max_radius, [&](entt::entity entity) {
            // Check diet
            // Note: We access registry inside callback, which is fine.
            // Need to check if it matches target diet
            const auto* diet = registry.try_get<DietComponent>(entity);
            if (!diet || diet->type != target_diet) return true;
            
            const auto& transform = registry.get<TransformComponent>(entity);
            const double dx = transform.position.x - center.x;
            const double dz = transform.position.z - center.z;
            const double dist_sq = dx*dx + dz*dz;
            
            if (dist_sq < min_dist_sq) {
                min_dist_sq = dist_sq;
                nearest = entity;
            }
            return true;
        });
        
        return nearest;
    }

private:
    double cell_size_;
    std::unordered_map<std::int64_t, std::vector<entt::entity>> cells_;
};

} // namespace evolution::sim
