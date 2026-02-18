#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

#include "evolution/sim/components.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

class CreatureSpatialIndex {
public:
    explicit CreatureSpatialIndex(double cell_size = 4.0) noexcept;

    void clear() noexcept;
    void rebuild(entt::registry& registry);

    template <typename Func>
    void for_each_in_radius(entt::registry& registry,
                            const Vec3& position,
                            double radius,
                            Func&& func) const;

    struct CellKey {
        int ix{0};
        int iz{0};

        [[nodiscard]] bool operator==(const CellKey& other) const noexcept {
            return ix == other.ix && iz == other.iz;
        }
    };

private:
    struct CellHasher {
        [[nodiscard]] std::size_t operator()(const CellKey& cell) const noexcept {
            const std::uint64_t x = static_cast<std::uint64_t>(cell.ix) * 73856093u;
            const std::uint64_t z = static_cast<std::uint64_t>(cell.iz) * 83492791u;
            return static_cast<std::size_t>(x ^ z);
        }
    };

    void insert(entt::entity entity, const Vec3& position);

    double cell_size_{4.0};
    double inv_cell_size_{0.25};
    std::unordered_map<CellKey, std::vector<entt::entity>, CellHasher> grid_{};
};

class CreatureSpatialSystem final : public ISystem {
public:
    CreatureSpatialSystem() = default;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "creature_spatial";
};

}  // namespace evolution::sim

template <typename Func>
void evolution::sim::CreatureSpatialIndex::for_each_in_radius(entt::registry& registry,
                                                              const Vec3& position,
                                                              double radius,
                                                              Func&& func) const {
    if (grid_.empty()) {
        return;
    }
    const double radius_sq = radius * radius;
    const int min_ix = static_cast<int>(std::floor(position.x * inv_cell_size_ - radius * inv_cell_size_));
    const int max_ix = static_cast<int>(std::floor(position.x * inv_cell_size_ + radius * inv_cell_size_));
    const int min_iz = static_cast<int>(std::floor(position.z * inv_cell_size_ - radius * inv_cell_size_));
    const int max_iz = static_cast<int>(std::floor(position.z * inv_cell_size_ + radius * inv_cell_size_));

    for (int iz = min_iz; iz <= max_iz; ++iz) {
        for (int ix = min_ix; ix <= max_ix; ++ix) {
            const CellKey key{ix, iz};
            const auto it = grid_.find(key);
            if (it == grid_.end()) {
                continue;
            }
            for (const entt::entity entity : it->second) {
                if (!registry.valid(entity)) {
                    continue;
                }
                const auto* transform = registry.try_get<TransformComponent>(entity);
                const auto* metabolism = registry.try_get<MetabolismComponent>(entity);
                if (transform == nullptr || metabolism == nullptr || metabolism->energy <= 0.0) {
                    continue;
                }
                const Vec3 delta = transform->position - position;
                const double dist_sq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
                if (dist_sq <= radius_sq) {
                    func(entity, dist_sq);
                }
            }
        }
    }
}
