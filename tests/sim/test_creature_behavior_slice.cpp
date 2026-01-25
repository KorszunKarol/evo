#include <algorithm>

#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/physics/simple_backend.h"
#include "evolution/sim/system_slices.h"

namespace evolution::sim {

TEST(SystemSlices, RegistersCreatureBehaviorSliceInOrder) {
    Scheduler scheduler;
    genetics::GenomeStorage storage;
    SimplePhysicsConfig physics_config{};
    SimplePhysicsBackend backend(physics_config);

    RegisterCreatureBehaviorSlice(scheduler, storage, backend);

    const auto names = scheduler.system_names();

    auto index_of = [&](std::string_view name) {
        const auto it = std::find(names.begin(), names.end(), name);
        return static_cast<int>(std::distance(names.begin(), it));
    };

    const int vision = index_of("vision");
    const int spatial = index_of("creature_spatial_index");
    const int social = index_of("social_behavior");
    const int brain = index_of("brain_inference");
    const int motor = index_of("motor");

    ASSERT_GE(vision, 0);
    ASSERT_GE(spatial, 0);
    ASSERT_GE(social, 0);
    ASSERT_GE(brain, 0);
    ASSERT_GE(motor, 0);

    EXPECT_LT(vision, spatial);
    EXPECT_LT(spatial, social);
    EXPECT_LT(social, brain);
    EXPECT_LT(brain, motor);
}

}  // namespace evolution::sim
