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
    backend_->sync_from_registry(context.registry());
    backend_->step(context.registry(), context.fixed_dt());
    stats_cache_ = backend_->stats();
    // Future hook: dispatch backend contact_events() to interested systems.
}

}  // namespace evolution::sim
