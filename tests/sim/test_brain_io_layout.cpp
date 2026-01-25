#include <gtest/gtest.h>

#include "evolution/sim/brain_io_layout.h"

namespace evolution::sim {

TEST(BrainIoLayout, DefaultsMatchPlan) {
    EXPECT_EQ(kBaseSensorCount, 8U);
    EXPECT_EQ(kVisionRayCapacity, 5U);
    EXPECT_EQ(kSocialSensorCount, 12U);
    EXPECT_EQ(kTotalInputCount, 30U);
}

}  // namespace evolution::sim
