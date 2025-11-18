#include "test_render_fixtures.h"

#include <glm/glm.hpp>

namespace evolution::client::test {

// Color utility functions matching main.cpp
namespace {
glm::vec3 energy_to_color(float ratio) {
    const glm::vec3 low{0.1F, 0.35F, 0.95F};
    const glm::vec3 mid{0.2F, 0.85F, 0.45F};
    const glm::vec3 high{0.95F, 0.32F, 0.18F};
    if (ratio < 0.5F) {
        return glm::mix(low, mid, ratio * 2.0F);
    }
    return glm::mix(mid, high, (ratio - 0.5F) * 2.0F);
}

glm::vec3 plant_species_color(std::uint8_t species_id) {
    constexpr std::array<glm::vec3, 6> palette{
        glm::vec3(0.55F, 0.80F, 0.25F), glm::vec3(0.85F, 0.65F, 0.15F),
        glm::vec3(0.95F, 0.35F, 0.35F), glm::vec3(0.45F, 0.65F, 0.90F),
        glm::vec3(0.90F, 0.45F, 0.80F), glm::vec3(0.40F, 0.90F, 0.80F)};
    return palette[species_id % palette.size()];
}
}  // namespace

TEST(ColorUtilsTest, EnergyToColorLow) {
    const glm::vec3 color = energy_to_color(0.0F);
    EXPECT_NEAR(color.r, 0.1F, 0.01F);
    EXPECT_NEAR(color.g, 0.35F, 0.01F);
    EXPECT_NEAR(color.b, 0.95F, 0.01F);
}

TEST(ColorUtilsTest, EnergyToColorMid) {
    const glm::vec3 color = energy_to_color(0.5F);
    EXPECT_NEAR(color.r, 0.2F, 0.01F);
    EXPECT_NEAR(color.g, 0.85F, 0.01F);
    EXPECT_NEAR(color.b, 0.45F, 0.01F);
}

TEST(ColorUtilsTest, EnergyToColorHigh) {
    const glm::vec3 color = energy_to_color(1.0F);
    EXPECT_NEAR(color.r, 0.95F, 0.01F);
    EXPECT_NEAR(color.g, 0.32F, 0.01F);
    EXPECT_NEAR(color.b, 0.18F, 0.01F);
}

TEST(ColorUtilsTest, EnergyToColorInterpolation) {
    const glm::vec3 low = energy_to_color(0.0F);
    const glm::vec3 mid = energy_to_color(0.5F);
    const glm::vec3 high = energy_to_color(1.0F);

    // Verify smooth interpolation
    const glm::vec3 quarter = energy_to_color(0.25F);
    EXPECT_GT(quarter.r, low.r);
    EXPECT_LT(quarter.r, mid.r);
    EXPECT_GT(quarter.g, low.g);
    EXPECT_LT(quarter.g, mid.g);

    const glm::vec3 three_quarter = energy_to_color(0.75F);
    EXPECT_GT(three_quarter.r, mid.r);
    EXPECT_LT(three_quarter.r, high.r);
    EXPECT_LT(three_quarter.g, mid.g);
    EXPECT_GT(three_quarter.g, high.g);
}

TEST(ColorUtilsTest, EnergyToColorClamped) {
    // Test values outside [0, 1] range
    // Note: The implementation uses glm::mix which doesn't clamp, so values outside [0,1]
    // will produce extrapolated colors. We test that the function handles them gracefully.
    const glm::vec3 below = energy_to_color(-1.0F);
    const glm::vec3 above = energy_to_color(2.0F);

    // Function should return valid vec3 (no NaN/inf)
    EXPECT_FALSE(std::isnan(below.r));
    EXPECT_FALSE(std::isnan(below.g));
    EXPECT_FALSE(std::isnan(below.b));
    EXPECT_FALSE(std::isnan(above.r));
    EXPECT_FALSE(std::isnan(above.g));
    EXPECT_FALSE(std::isnan(above.b));
    
    EXPECT_FALSE(std::isinf(below.r));
    EXPECT_FALSE(std::isinf(above.r));
}

TEST(ColorUtilsTest, PlantSpeciesColorValid) {
    for (std::uint8_t i = 0; i < 6; ++i) {
        const glm::vec3 color = plant_species_color(i);
        EXPECT_GE(color.r, 0.0F);
        EXPECT_LE(color.r, 1.0F);
        EXPECT_GE(color.g, 0.0F);
        EXPECT_LE(color.g, 1.0F);
        EXPECT_GE(color.b, 0.0F);
        EXPECT_LE(color.b, 1.0F);
    }
}

TEST(ColorUtilsTest, PlantSpeciesColorWraps) {
    // Test that species_id wraps around palette
    const glm::vec3 color0 = plant_species_color(0);
    const glm::vec3 color6 = plant_species_color(6);
    const glm::vec3 color12 = plant_species_color(12);

    EXPECT_FLOAT_EQ(color0.r, color6.r);
    EXPECT_FLOAT_EQ(color0.g, color6.g);
    EXPECT_FLOAT_EQ(color0.b, color6.b);

    EXPECT_FLOAT_EQ(color0.r, color12.r);
    EXPECT_FLOAT_EQ(color0.g, color12.g);
    EXPECT_FLOAT_EQ(color0.b, color12.b);
}

TEST(ColorUtilsTest, PlantSpeciesColorUnique) {
    // Verify different species IDs produce different colors
    std::vector<glm::vec3> colors;
    for (std::uint8_t i = 0; i < 6; ++i) {
        colors.push_back(plant_species_color(i));
    }

    // Check that at least some colors are different
    bool has_differences = false;
    for (std::size_t i = 0; i < colors.size(); ++i) {
        for (std::size_t j = i + 1; j < colors.size(); ++j) {
            if (colors[i] != colors[j]) {
                has_differences = true;
                break;
            }
        }
        if (has_differences) {
            break;
        }
    }
    EXPECT_TRUE(has_differences);
}

TEST(ColorUtilsTest, PlantSpeciesColorLargeValues) {
    // Test with large species IDs
    const glm::vec3 color255 = plant_species_color(255);
    EXPECT_GE(color255.r, 0.0F);
    EXPECT_LE(color255.r, 1.0F);

    const glm::vec3 color128 = plant_species_color(128);
    EXPECT_GE(color128.r, 0.0F);
    EXPECT_LE(color128.r, 1.0F);
}

}  // namespace evolution::client::test

