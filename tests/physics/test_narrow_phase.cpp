#include <gtest/gtest.h>

#include "evolution/sim/physics/narrow_phase.h"

using namespace evolution::sim;

TEST(NarrowPhase, SphereSpherePenetration) {
    ContactManifold manifold{};
    const Vec3 center_a{0.0, 0.0, 0.0};
    const Vec3 center_b{0.5, 0.0, 0.0};
    const double radius = 0.5;

    const bool collided = collide_sphere_sphere(entt::entity{1},
                                                entt::entity{2},
                                                center_a,
                                                radius,
                                                center_b,
                                                radius,
                                                manifold);

    ASSERT_TRUE(collided);
    EXPECT_EQ(manifold.count, 1u);
    EXPECT_NEAR(manifold.points[0].penetration, 0.5, 1e-6);
    EXPECT_NEAR(manifold.points[0].normal.x, 1.0, 1e-6);
}

TEST(NarrowPhase, SphereAabbDetection) {
    ContactManifold manifold{};
    const Vec3 center{0.25, 0.25, 0.0};
    const double radius = 0.5;
    const Vec3 box_min{-0.5, -0.5, -0.5};
    const Vec3 box_max{0.5, 0.5, 0.5};

    const bool collided = collide_sphere_aabb(entt::entity{1},
                                              entt::entity{2},
                                              center,
                                              radius,
                                              box_min,
                                              box_max,
                                              manifold);

    ASSERT_TRUE(collided);
    EXPECT_EQ(manifold.count, 1u);
    EXPECT_GT(manifold.points[0].penetration, 0.0);
}


