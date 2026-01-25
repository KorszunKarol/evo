#include "evolution/sim/creature_spatial_index_system.h"

namespace evolution::sim {

CreatureSpatialIndexSystem::CreatureSpatialIndexSystem(double cell_size) noexcept
    : cell_size_(cell_size) {}

void CreatureSpatialIndexSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto* index_ptr = registry.ctx().find<CreatureSpatialIndex>();
    if (index_ptr == nullptr) {
        index_ptr = &registry.ctx().emplace<CreatureSpatialIndex>(cell_size_);
    }
    index_ptr->rebuild(registry);
}

}  // namespace evolution::sim
