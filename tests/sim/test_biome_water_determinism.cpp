#include "test_fixtures.h"

#include <array>
#include <unordered_map>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/environment_bootstrap.h"

using namespace evolution::sim::test;
using namespace evolution::sim;

TEST(BiomeMap, DeterministicSameSeed) {
    EnvironmentConfig config1 = create_test_env_config(2025);
    EnvironmentConfig config2 = create_test_env_config(2025);

    SimulationFixture fixture1, fixture2;
    initialize_environment(fixture1.app().registry(), config1);
    initialize_environment(fixture2.app().registry(), config2);

    auto& registry1 = fixture1.app().registry();
    auto& registry2 = fixture2.app().registry();

    const auto& biome_map1 = registry1.ctx().get<BiomeMap>();
    const auto& biome_map2 = registry2.ctx().get<BiomeMap>();

    // Sample same positions
    constexpr double test_positions[][2] = {
        {10.0, 20.0}, {50.0, 30.0}, {100.0, 150.0}, {200.0, 50.0}};

    for (const auto& pos : test_positions) {
        const BiomeId biome1 = biome_map1.sample(pos[0], pos[1]);
        const BiomeId biome2 = biome_map2.sample(pos[0], pos[1]);
        EXPECT_EQ(biome1, biome2) << "Position (" << pos[0] << ", " << pos[1]
                                   << ") should have same biome";
    }
}

TEST(BiomeMap, DifferentSeedsProduceDifferentMaps) {
    EnvironmentConfig config1 = create_test_env_config(2025);
    EnvironmentConfig config2 = create_test_env_config(2026);

    SimulationFixture fixture1, fixture2;
    initialize_environment(fixture1.app().registry(), config1);
    initialize_environment(fixture2.app().registry(), config2);

    auto& registry1 = fixture1.app().registry();
    auto& registry2 = fixture2.app().registry();

    const auto& biome_map1 = registry1.ctx().get<BiomeMap>();
    const auto& biome_map2 = registry2.ctx().get<BiomeMap>();

    // Check multiple positions - at least some should differ
    int differences = 0;
    for (double x = 0.0; x < 200.0; x += 20.0) {
        for (double z = 0.0; z < 200.0; z += 20.0) {
            if (biome_map1.sample(x, z) != biome_map2.sample(x, z)) {
                ++differences;
            }
        }
    }

    EXPECT_GT(differences, 0) << "Different seeds should produce different biome maps";
}

TEST(BiomeMap, ValidBiomeIds) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& biome_map = registry.ctx().get<BiomeMap>();

    // Sample across entire map
    for (double x = 0.0; x < 250.0; x += 10.0) {
        for (double z = 0.0; z < 250.0; z += 10.0) {
            const BiomeId biome = biome_map.sample(x, z);
            EXPECT_GE(static_cast<int>(biome), 0);
            EXPECT_LE(static_cast<int>(biome), 3);  // Plains, Forest, Wetland, Alpine
        }
    }
}

TEST(BiomeMap, GeneratesAllRequestedBiomes) {
    EnvironmentConfig config = create_test_env_config(2025);
    config.biome.biome_count = 4;
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& biome_map = registry.ctx().get<BiomeMap>();
    const auto& terrain = registry.ctx().get<Terrain>();

    std::array<bool, 4> seen{};
    const double max_x = static_cast<double>(terrain.width()) * terrain.cell_size();
    const double max_z = static_cast<double>(terrain.height_cells()) * terrain.cell_size();

    for (double x = 0.0; x < max_x; x += 5.0) {
        for (double z = 0.0; z < max_z; z += 5.0) {
            const BiomeId biome = biome_map.sample(x, z);
            const int idx = std::clamp(static_cast<int>(biome), 0, 3);
            seen[idx] = true;
        }
    }

    for (int i = 0; i < config.biome.biome_count; ++i) {
        EXPECT_TRUE(seen[i]) << "Requested biome " << i << " should appear in the map";
    }
}

TEST(WaterMap, DeterministicSameSeed) {
    EnvironmentConfig config1 = create_test_env_config(2025);
    EnvironmentConfig config2 = create_test_env_config(2025);

    SimulationFixture fixture1, fixture2;
    initialize_environment(fixture1.app().registry(), config1);
    initialize_environment(fixture2.app().registry(), config2);

    auto& registry1 = fixture1.app().registry();
    auto& registry2 = fixture2.app().registry();

    const auto& water_map1 = registry1.ctx().get<WaterMap>();
    const auto& water_map2 = registry2.ctx().get<WaterMap>();

    constexpr double test_positions[][2] = {
        {10.0, 20.0}, {50.0, 30.0}, {100.0, 150.0}, {200.0, 50.0}};

    for (const auto& pos : test_positions) {
        const double depth1 = water_map1.depth(pos[0], pos[1]);
        const double depth2 = water_map2.depth(pos[0], pos[1]);
        EXPECT_DOUBLE_EQ(depth1, depth2) << "Position (" << pos[0] << ", " << pos[1]
                                         << ") should have same depth";
    }
}

TEST(WaterMap, DepthNonNegative) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& water_map = registry.ctx().get<WaterMap>();

    for (double x = 0.0; x < 250.0; x += 10.0) {
        for (double z = 0.0; z < 250.0; z += 10.0) {
            const double depth = water_map.depth(x, z);
            EXPECT_GE(depth, 0.0) << "Depth should be non-negative at (" << x << ", " << z << ")";
        }
    }
}

TEST(WaterMap, ShoreDistanceNonNegative) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& water_map = registry.ctx().get<WaterMap>();

    for (double x = 0.0; x < 250.0; x += 10.0) {
        for (double z = 0.0; z < 250.0; z += 10.0) {
            const double dist = water_map.shore_distance(x, z);
            EXPECT_GE(dist, 0.0) << "Shore distance should be non-negative";
        }
    }
}

TEST(WaterMap, IsWaterConsistent) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& water_map = registry.ctx().get<WaterMap>();

    for (double x = 0.0; x < 250.0; x += 10.0) {
        for (double z = 0.0; z < 250.0; z += 10.0) {
            const double depth = water_map.depth(x, z);
            const bool is_water = water_map.is_water(x, z);
            EXPECT_EQ(is_water, depth > 0.0) << "is_water should match depth > 0";
        }
    }
}

TEST(WaterMap, WaterLevelConsistent) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& water_map = registry.ctx().get<WaterMap>();
    const auto& terrain = registry.ctx().get<Terrain>();

    const double water_level = water_map.water_level();
    EXPECT_GE(water_level, terrain.min_y());
    EXPECT_LE(water_level, terrain.max_y());
}

TEST(BiomeWaterIntegration, BiomeAffectsSoilRegen) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    auto& soil = registry.ctx().get<SoilGrid>();
    const auto& biome_map = registry.ctx().get<BiomeMap>();

    // Sample soil at different biomes
    std::unordered_map<BiomeId, double> biome_soil_sum;
    std::unordered_map<BiomeId, int> biome_count;

    for (double x = 0.0; x < 200.0; x += 5.0) {
        for (double z = 0.0; z < 200.0; z += 5.0) {
            const BiomeId biome = biome_map.sample(x, z);
            const float soil_val = soil.sample(x, z);
            biome_soil_sum[biome] += static_cast<double>(soil_val);
            ++biome_count[biome];
        }
    }

    // Each biome should have some soil
    for (const auto& [biome, count] : biome_count) {
        EXPECT_GT(count, 0) << "Biome " << static_cast<int>(biome) << " should exist";
        const double mean = biome_soil_sum[biome] / static_cast<double>(count);
        EXPECT_GT(mean, 0.0) << "Biome " << static_cast<int>(biome) << " should have soil";
    }
}

TEST(BiomeWaterIntegration, AtLeastSomeLandAboveWater) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& water_map = registry.ctx().get<WaterMap>();

    int land_cells = 0;
    int water_cells = 0;

    for (double x = 0.0; x < 250.0; x += 5.0) {
        for (double z = 0.0; z < 250.0; z += 5.0) {
            if (water_map.is_water(x, z)) {
                ++water_cells;
            } else {
                ++land_cells;
            }
        }
    }

    const double land_fraction = static_cast<double>(land_cells) /
                                 static_cast<double>(land_cells + water_cells);
    EXPECT_GT(land_fraction, 0.2) << "At least 20% should be land";
}

