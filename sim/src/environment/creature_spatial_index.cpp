#include "evolution/sim/environment/creature_spatial_index.h"

#include <algorithm>

namespace evolution::sim {

CreatureSpatialIndex::CreatureSpatialIndex(double cell_size) noexcept
    : cell_size_(std::max(cell_size, 0.1)),
      inv_cell_size_(1.0 / cell_size_) {}

void CreatureSpatialIndex::clear() noexcept {
    grid_.clear();
}

void CreatureSpatialIndex::rebuild(entt::registry& registry) {
    grid_.clear();
    auto view = registry.view<TransformComponent, MetabolismComponent>();
    for (auto entity : view) {
        const auto& metabolism = view.get<MetabolismComponent>(entity);
        if (metabolism.energy <= 0.0) {
            continue;
        }
        const auto& transform = view.get<TransformComponent>(entity);
        insert(entity, transform.position);
    }
}

void CreatureSpatialIndex::insert(entt::entity entity, const Vec3& position) {
    const int ix = static_cast<int>(std::floor(position.x * inv_cell_size_));
    const int iz = static_cast<int>(std::floor(position.z * inv_cell_size_));
    const CellKey key{ix, iz};
    grid_[key].push_back(entity);
}

void CreatureSpatialSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto* index = registry.ctx().find<CreatureSpatialIndex>();
    if (index == nullptr) {
        return;
    }
    index->rebuild(registry);
}

}  // namespace evolution::sim
