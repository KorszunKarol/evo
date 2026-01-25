#pragma once

#include <string_view>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/physics/backend.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

class MetabolismSystem;

/**
 * @brief Holds pointers to systems created by a slice registration helper.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1).
 * @note Pointers are owned by the scheduler and remain valid while it lives.
 * @warning Do not delete or store beyond scheduler lifetime.
 * @threadsafe @notthreadsafe.
 */
struct CreatureBehaviorSliceHandles {
    MetabolismSystem* metabolism{nullptr};
};

/**
 * @brief Registers the shared creature-behavior systems in a stable order.
 * @param scheduler Scheduler& Target scheduler to register systems into.
 * @param storage genetics::GenomeStorage& Genome storage for brain inference.
 * @param backend const IPhysicsBackend& Physics backend for vision raycasts.
 * @return CreatureBehaviorSliceHandles Pointers to created systems for optional wiring.
 * @throws None.
 * @complexity O(1) registration plus O(1) allocations for system instances.
 * @note Order is fixed to preserve deterministic behavior.
 * @warning Call once during setup; do not interleave with per-tick mutations.
 * @threadsafe @notthreadsafe.
 */
CreatureBehaviorSliceHandles RegisterCreatureBehaviorSlice(Scheduler& scheduler,
                                                           genetics::GenomeStorage& storage,
                                                           const IPhysicsBackend& backend);

}  // namespace evolution::sim
