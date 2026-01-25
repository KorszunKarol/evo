#include "evolution/sim/physics_system.h"

#include <stdexcept>

namespace evolution::sim {

PhysicsSystem::PhysicsSystem(std::unique_ptr<IPhysicsBackend> backend)
    : backend_(std::move(backend)) {
    if (!backend_) {
        throw std::invalid_argument("PhysicsSystem requires a valid backend instance");
    }
}

void PhysicsSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    
    // PHASE 3: Expose backend to context for other systems (e.g. FeedingSystem)
    // to query contact events.
    registry.ctx().insert_or_assign<IPhysicsBackend*>(backend_.get());

    backend_->sync_from_registry(registry);
    backend_->step(registry, context.fixed_dt());
    stats_cache_ = backend_->stats();
}

}  // namespace evolution::sim
