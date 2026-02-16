#include <algorithm>

#include <gtest/gtest.h>

#include "evolution/sim/environment/service_adapters.h"
#include "test_fixtures.h"

using namespace evolution::sim;
using namespace evolution::sim::test;

class RuntimeContractsFixture final : public SimulationFixture {
public:
    void Initialize() { SetUp(); }
};

TEST(RuntimeContracts, WorldTimeFromSimulationApp) {
    SimulationApp app;
    EXPECT_EQ(app.tick_count(), 0u);
    app.tick();
    EXPECT_EQ(app.tick_count(), 1u);
    EXPECT_GT(app.simulation_time(), 0.0);
}

TEST(RuntimeContracts, SoilGridFieldSampleAndConsume) {
    SoilConfig config{};
    config.width_cells = 8;
    config.height_cells = 8;
    config.cell_size = 1.0;
    config.baseline_nutrient = 5.0F;
    SoilGrid grid(config);
    SoilGridField field(&grid);

    const double before = field.sample_nutrient(2.0, 3.0);
    field.consume_nutrient(2.0, 3.0, 1.25);
    const double after = field.sample_nutrient(2.0, 3.0);

    EXPECT_GT(before, after);
}

TEST(RuntimeContracts, SoilVolumeFieldSampleAndConsume) {
    SoilVolumeConfig config{};
    config.width = 8;
    config.height = 4;
    config.depth = 8;
    config.voxel_size = 1.0;
    SoilVolume volume(config);
    volume.at(2, 0, 3).nitrogen = 4.0F;
    SoilVolumeField field(&volume);

    const double before = field.sample_nutrient(2.0, 3.0);
    field.consume_nutrient(2.0, 3.0, 1.5);
    const double after = field.sample_nutrient(2.0, 3.0);

    EXPECT_GT(before, after);
}

TEST(RuntimeContracts, PlantSpatialQueryAdapterFindsNearbyPlant) {
    RuntimeContractsFixture fixture;
    fixture.Initialize();
    auto& registry = fixture.app().registry();

    const entt::entity plant = fixture.spawn_plant(Vec3{10.0, 0.0, 10.0}, 0, 10.0);
    auto& index = registry.ctx().get<PlantSpatialIndex>();
    index.rebuild(registry);

    PlantSpatialQueryAdapter adapter(&index, &registry);
    const auto near = adapter.query_radius(10.0, 10.0, 2.0);
    const auto far = adapter.query_radius(100.0, 100.0, 2.0);

    EXPECT_FALSE(near.empty());
    EXPECT_NE(std::find(near.begin(), near.end(), plant), near.end());
    EXPECT_TRUE(far.empty());
}
