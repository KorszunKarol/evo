#include <gtest/gtest.h>

#include "evolution/sim/environment/soil_volume.h"

using namespace evolution::sim;
using namespace evolution::sim::math;

TEST(SoilVolumeTest, Initialization) {
    SoilVolumeConfig config;
    config.width = 10;
    config.height = 5;
    config.depth = 10;
    
    SoilVolume volume(config);
    
    EXPECT_EQ(volume.width(), 10);
    EXPECT_EQ(volume.height(), 5);
    EXPECT_EQ(volume.depth(), 10);
    
    const auto& v = volume.at(0, 0, 0);
    EXPECT_EQ(v.nitrogen.raw(), 0);
}

TEST(SoilVolumeTest, AccessAndModification) {
    SoilVolumeConfig config;
    SoilVolume volume(config);
    
    volume.at(1, 1, 1).nitrogen = Fixed64(10.0);
    EXPECT_DOUBLE_EQ(volume.at(1, 1, 1).nitrogen.to_double(), 10.0);
}

TEST(SoilVolumeTest, Sampling) {
    SoilVolumeConfig config;
    config.width = 4;
    config.height = 4;
    config.depth = 4;
    config.voxel_size = 1.0;
    
    SoilVolume volume(config);
    
    // Set values to create a gradient
    // (0,0,0) -> 0
    // (1,0,0) -> 10
    volume.at(0, 0, 0).nitrogen = Fixed64(0.0);
    volume.at(1, 0, 0).nitrogen = Fixed64(10.0);
    
    // Sample halfway
    Vec3 pos{0.5, 0.0, 0.0};
    SoilVoxel sample = volume.sample(pos);
    
    EXPECT_NEAR(sample.nitrogen.to_double(), 5.0, 0.001);
    
    // Sample exactly at node
    Vec3 pos2{1.0, 0.0, 0.0};
    SoilVoxel sample2 = volume.sample(pos2);
    EXPECT_NEAR(sample2.nitrogen.to_double(), 10.0, 0.001);
}

TEST(SoilVolumeTest, Diffusion) {
    SoilVolumeConfig config;
    config.width = 5;
    config.height = 5;
    config.depth = 5;
    config.diffusion_rate = Fixed64(0.1);
    
    SoilVolume volume(config);
    
    // Set center to high value
    volume.at(2, 2, 2).nitrogen = Fixed64(10.0);
    
    // Run diffusion
    volume.diffuse(Fixed64(1.0));
    
    // Center should decrease, neighbors increase
    EXPECT_LT(volume.at(2, 2, 2).nitrogen.to_double(), 10.0);
    EXPECT_GT(volume.at(1, 2, 2).nitrogen.to_double(), 0.0); // -X neighbor
    EXPECT_GT(volume.at(3, 2, 2).nitrogen.to_double(), 0.0); // +X neighbor
}

