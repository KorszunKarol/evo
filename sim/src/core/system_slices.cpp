#include "evolution/sim/system_slices.h"

#include "evolution/sim/brain_inference_system.h"
#include "evolution/sim/creature_spatial_index_system.h"
#include "evolution/sim/decomposition_system.h"
#include "evolution/sim/metabolism_system.h"
#include "evolution/sim/motor_system.h"
#include "evolution/sim/social_behavior_system.h"
#include "evolution/sim/vision_system.h"

namespace evolution::sim {

CreatureBehaviorSliceHandles RegisterCreatureBehaviorSlice(Scheduler& scheduler,
                                                           genetics::GenomeStorage& storage,
                                                           const IPhysicsBackend& backend) {
    scheduler.add_system(std::make_unique<VisionSystem>(backend));
    scheduler.add_system(std::make_unique<CreatureSpatialIndexSystem>());
    scheduler.add_system(std::make_unique<SocialBehaviorSystem>());
    scheduler.add_system(std::make_unique<BrainInferenceSystem>(storage));
    scheduler.add_system(std::make_unique<MotorSystem>());

    scheduler.add_system(std::make_unique<DecompositionSystem>());

    auto metabolism = std::make_unique<MetabolismSystem>();
    MetabolismSystem* metabolism_ptr = metabolism.get();
    scheduler.add_system(std::move(metabolism));

    return CreatureBehaviorSliceHandles{.metabolism = metabolism_ptr};
}

}  // namespace evolution::sim
