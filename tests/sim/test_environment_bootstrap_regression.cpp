#include "test_fixtures.h"

#include <gtest/gtest.h>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/environment_bootstrap.h"

using namespace evolution::sim;
using namespace evolution::sim::test;

TEST(EnvironmentBootstrapRegression, TerrainNormalsPointUpward) {
    EnvironmentConfig config = create_test_env_config(2025);

    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    const auto& terrain = fixture.app().registry().ctx().get<Terrain>();

    // Sample a small grid of points and ensure normals point upward.
    for (double x = 10.0; x <= 100.0; x += 15.0) {
        for (double z = 10.0; z <= 100.0; z += 15.0) {
            const Vec3 normal = terrain.normal(x, z);
            EXPECT_GT(normal.y, 0.0) << "Normal should have positive Y at (" << x << ", " << z << ")";
        }
    }
}

TEST(EnvironmentBootstrapRegression, MainScenarioConfigSeedsPlants) {
    EnvironmentConfig config{};
    config.terrain.width_cells = 512;
    config.terrain.height_cells = 512;
    config.terrain.cell_size = 2.0;
    config.terrain.elevation_scale = 18.0;
    config.terrain.octaves = 5;
    config.terrain.base_frequency = 0.004;
    config.terrain.seed = 2025;

    config.soil.width_cells = 256;
    config.soil.height_cells = 256;
    config.soil.cell_size = 4.0;
    config.soil.max_nutrient = 12.0F;
    config.soil.diffusion_rate = 0.45F;
    config.soil.regeneration_rate = 0.08F;
    config.soil.baseline_nutrient = 5.0F;

    config.plants.initial_count = 800;
    config.plants.min_initial_energy = 6.0;
    config.plants.max_initial_energy = 16.0;
    config.plants.seed = 1337;
    config.plant_spatial_cell_size = 4.0;

    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);
    seed_initial_plants(fixture.app().registry(), config);

    std::size_t alive_plants = 0;
    auto view = fixture.app().registry().view<PlantComponent>();
    view.each([&](const PlantComponent& plant) {
        if (plant.alive) {
            ++alive_plants;
        }
    });

    EXPECT_GT(alive_plants, 0u);
}
