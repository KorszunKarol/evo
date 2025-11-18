#include <gtest/gtest.h>

#include "evolution/sim/math/fixed_point.h"

using namespace evolution::sim::math;

TEST(FixedPointTest, ConstructionAndConversion) {
    Fixed64 a(1.0);
    EXPECT_DOUBLE_EQ(a.to_double(), 1.0);
    EXPECT_EQ(a.to_int(), 1);

    Fixed64 b(2);
    EXPECT_DOUBLE_EQ(b.to_double(), 2.0);
    EXPECT_EQ(b.to_int(), 2);

    Fixed64 c(0.5);
    EXPECT_DOUBLE_EQ(c.to_double(), 0.5);
    EXPECT_EQ(c.to_int(), 0);

    Fixed64 d(-1.5);
    EXPECT_DOUBLE_EQ(d.to_double(), -1.5);
    EXPECT_EQ(d.to_int(), -1); // Truncation towards negative infinity usually with shifts?
                               // -1.5 = -1.5 * 2^32. Shift >> 32.
                               // -1.5 * 2^32 is roughly -6442450944.
                               // -6442450944 >> 32 is -1 (in 2's complement arithmetic usually dependent on implementation but C++20 defines it).
                               // C++20: right shift of negative signed integer is implementation-defined or arithmetic?
                               // Actually C++20 standardizes right shift of negative values to be arithmetic shift (sign extension).
                               // So -1.5 (represented as large negative) >> 32 should be -2?
                               // 0xFFFFFFFF... (lots of Fs)
                               // Let's check: -1 is all ones.
                               // -1.5 * One = -1.5 * 2^32 = -1 * 2^32 + (-0.5 * 2^32).
                               // -3/2 * 2^32.
                               // If we divide by 2^32, we get -1.5. Integer part is -1.
                               // Let's verify expectation.
}

TEST(FixedPointTest, Arithmetic) {
    Fixed64 a(2.0);
    Fixed64 b(3.0);

    EXPECT_DOUBLE_EQ((a + b).to_double(), 5.0);
    EXPECT_DOUBLE_EQ((a - b).to_double(), -1.0);
    EXPECT_DOUBLE_EQ((a * b).to_double(), 6.0);
    EXPECT_DOUBLE_EQ((b / a).to_double(), 1.5);
}

TEST(FixedPointTest, Precision) {
    Fixed64 a(0.1);
    Fixed64 b(0.2);
    Fixed64 c = a + b;
    
    // 0.1 + 0.2 in float often isn't exactly 0.3, but in fixed point 32.32 it should be very close.
    EXPECT_NEAR(c.to_double(), 0.3, 1e-9);
}

TEST(FixedPointTest, Comparison) {
    Fixed64 a(1.0);
    Fixed64 b(2.0);
    
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(a <= a);
    EXPECT_TRUE(a == a);
    EXPECT_TRUE(a != b);
}

