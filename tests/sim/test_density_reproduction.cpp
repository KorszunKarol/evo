#include <gtest/gtest.h>

#include "evolution/sim/environment/creature_spatial_index.h"
#include "evolution/sim/population_monitor.h"
#include "evolution/sim/reproduction_system.h"
#include "test_fixtures.h"

using namespace evolution::sim;
using namespace evolution::sim::test;

class DensityReproductionFixture : public SimulationFixture {
public:
    void Initialize() { SetUp(); }
};

TEST(DensityReproduction, HighDensityIncreasesThresholdAndBlocksReproduction) {
    DensityReproductionFixture fixture;
    fixture.Initialize();

    auto& registry = fixture.app().registry();
    const auto gid = fixture.create_test_genome(1001);
    const entt::entity parent = fixture.spawn_herbivore({0.0, 0.0, 0.0}, gid);

    auto& parent_repro = registry.get<ReproductionComponent>(parent);
    parent_repro.timer = 0.0;
    parent_repro.energy_threshold = 50.0;
    parent_repro.density_sensitivity = 2.0;
    parent_repro.ideal_local_density = 2.0;
    parent_repro.density_query_radius = 3.0;
    parent_repro.critical_density_pressure = 0.5;
    auto& parent_met = registry.get<MetabolismComponent>(parent);
    parent_met.energy = 120.0;

    for (int i = 0; i < 10; ++i) {
        const entt::entity e = registry.create();
        registry.emplace<TransformComponent>(e, TransformComponent{Vec3{0.3 * i, 0.0, 0.0}});
        registry.emplace<MetabolismComponent>(e, MetabolismComponent{.energy = 20.0, .max_energy = 20.0, .basal_rate = 0.0});
    }

    registry.ctx().get<CreatureSpatialIndex>().rebuild(registry);

    ReproductionSystem system(fixture.storage(), evolution::genetics::ReproConfig{}, 42);
    system.set_asexual_fallback(true);

    const std::size_t before = registry.storage<entt::entity>().in_use();
    SimulationContext ctx(registry, 1.0 / 60.0, 0.0);
    system.tick(ctx);
    const std::size_t after = registry.storage<entt::entity>().in_use();

    EXPECT_EQ(after, before);
}

TEST(DensityReproduction, LowDensityDoesNotPenalizeAsexualFallback) {
    DensityReproductionFixture fixture;
    fixture.Initialize();

    auto& registry = fixture.app().registry();
    const auto gid = fixture.create_test_genome(2002);
    const entt::entity parent = fixture.spawn_herbivore({0.0, 0.0, 0.0}, gid);

    auto& parent_repro = registry.get<ReproductionComponent>(parent);
    parent_repro.timer = 0.0;
    parent_repro.energy_threshold = 40.0;
    parent_repro.density_sensitivity = 1.0;
    parent_repro.ideal_local_density = 8.0;
    parent_repro.density_query_radius = 3.0;
    parent_repro.critical_density_pressure = 10.0;
    auto& parent_met = registry.get<MetabolismComponent>(parent);
    parent_met.energy = 120.0;

    registry.ctx().get<CreatureSpatialIndex>().rebuild(registry);

    ReproductionSystem system(fixture.storage(), evolution::genetics::ReproConfig{}, 42);
    system.set_asexual_fallback(true);

    const std::size_t before = registry.storage<entt::entity>().in_use();
    SimulationContext ctx(registry, 1.0 / 60.0, 0.0);
    system.tick(ctx);
    const std::size_t after = registry.storage<entt::entity>().in_use();

    EXPECT_GT(after, before);
}

TEST(DensityReproduction, DensityQueryRespectsRadius) {
    DensityReproductionFixture fixture;
    fixture.Initialize();

    auto& registry = fixture.app().registry();
    const auto gid = fixture.create_test_genome(3003);
    const entt::entity parent = fixture.spawn_herbivore({0.0, 0.0, 0.0}, gid);

    auto& parent_repro = registry.get<ReproductionComponent>(parent);
    parent_repro.timer = 0.0;
    parent_repro.energy_threshold = 40.0;
    parent_repro.density_sensitivity = 2.0;
    parent_repro.ideal_local_density = 1.0;
    parent_repro.density_query_radius = 1.0;
    parent_repro.critical_density_pressure = 0.5;
    auto& parent_met = registry.get<MetabolismComponent>(parent);
    parent_met.energy = 120.0;

    const entt::entity far_dummy = registry.create();
    registry.emplace<TransformComponent>(far_dummy, TransformComponent{Vec3{5.0, 0.0, 0.0}});
    registry.emplace<MetabolismComponent>(far_dummy, MetabolismComponent{});

    registry.ctx().get<CreatureSpatialIndex>().rebuild(registry);

    ReproductionSystem system(fixture.storage(), evolution::genetics::ReproConfig{}, 42);
    system.set_asexual_fallback(true);

    const std::size_t before = registry.storage<entt::entity>().in_use();
    SimulationContext ctx(registry, 1.0 / 60.0, 0.0);
    system.tick(ctx);
    const std::size_t after = registry.storage<entt::entity>().in_use();

    EXPECT_GT(after, before);
}
