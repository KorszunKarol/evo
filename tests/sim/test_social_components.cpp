#include <type_traits>

#include <gtest/gtest.h>

#include "evolution/sim/components.h"

namespace evolution::sim {

static_assert(std::is_standard_layout_v<TerritoryComponent>);
static_assert(std::is_trivially_copyable_v<TerritoryComponent>);
static_assert(std::is_standard_layout_v<SocialSignalsComponent>);
static_assert(std::is_trivially_copyable_v<SocialSignalsComponent>);

TEST(SocialComponents, DefaultsAreDeterministic) {
    TerritoryComponent territory;
    EXPECT_FALSE(territory.initialized);
    EXPECT_DOUBLE_EQ(territory.radius, 0.0);
    EXPECT_DOUBLE_EQ(territory.center.x, 0.0);
    EXPECT_DOUBLE_EQ(territory.center.y, 0.0);
    EXPECT_DOUBLE_EQ(territory.center.z, 0.0);

    SocialSignalsComponent signals;
    EXPECT_DOUBLE_EQ(signals.neighbor_density, 0.0);
    EXPECT_DOUBLE_EQ(signals.territory_dist_norm, 0.0);
    EXPECT_DOUBLE_EQ(signals.intruder_density, 0.0);
    EXPECT_DOUBLE_EQ(signals.pack_density_near_prey, 0.0);
}

}  // namespace evolution::sim
