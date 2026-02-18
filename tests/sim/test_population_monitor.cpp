#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/components.h"
#include "evolution/sim/population_monitor.h"
#include "evolution/sim/simulation_context.h"

using namespace evolution::sim;

namespace {

entt::entity spawn_creature(entt::registry& registry, bool herbivore, bool carnivore) {
    const entt::entity e = registry.create();
    registry.emplace<MetabolismComponent>(e);
    if (herbivore) {
        registry.emplace<HerbivoreTag>(e);
    }
    if (carnivore) {
        registry.emplace<CarnivoreTag>(e);
    }
    return e;
}

entt::entity spawn_plant(entt::registry& registry, bool alive) {
    const entt::entity e = registry.create();
    registry.emplace<PlantComponent>(e, PlantComponent{.alive = alive});
    return e;
}

}  // namespace

TEST(PopulationMonitor, CountsCreatureAndPlantBuckets) {
    entt::registry registry;
    evolution::genetics::GenomeStorage storage;

    PopulationConfig config{};
    config.world_area_override = 100.0;
    PopulationSystem system(storage, config, 123);

    spawn_creature(registry, true, false);
    spawn_creature(registry, true, false);
    spawn_creature(registry, false, true);
    spawn_plant(registry, true);
    spawn_plant(registry, false);

    SimulationContext ctx(registry, 1.0 / 60.0, 0.0);
    system.tick(ctx);

    const auto& monitor = registry.ctx().get<PopulationMonitor>();
    EXPECT_EQ(monitor.latest.creature_count, 3u);
    EXPECT_EQ(monitor.latest.herbivore_count, 2u);
    EXPECT_EQ(monitor.latest.carnivore_count, 1u);
    EXPECT_EQ(monitor.latest.plant_count, 1u);
    EXPECT_DOUBLE_EQ(monitor.latest.creature_density, 0.03);
}

TEST(PopulationMonitor, EmaConvergesForSteadyPopulation) {
    entt::registry registry;
    evolution::genetics::GenomeStorage storage;

    PopulationConfig config{};
    config.monitor_ema_window = 30;
    config.world_area_override = 1.0;
    PopulationSystem system(storage, config, 456);

    for (int i = 0; i < 20; ++i) {
        spawn_creature(registry, true, false);
    }

    for (int i = 0; i < 80; ++i) {
        SimulationContext ctx(registry, 1.0 / 60.0, static_cast<double>(i) / 60.0);
        system.tick(ctx);
    }

    const auto& monitor = registry.ctx().get<PopulationMonitor>();
    EXPECT_NEAR(monitor.latest.ema_creature_count, 20.0, 0.2);
}

TEST(PopulationMonitor, ThresholdFlagsAreEdgeAccurate) {
    entt::registry registry;
    evolution::genetics::GenomeStorage storage;

    PopulationConfig config{};
    config.min_viable_population = 3;
    config.max_carry_capacity = 4;
    config.hard_cap = 6;
    config.world_area_override = 1.0;
    PopulationSystem system(storage, config, 789);

    spawn_creature(registry, true, false);
    spawn_creature(registry, true, false);

    SimulationContext ctx_a(registry, 1.0 / 60.0, 0.0);
    system.tick(ctx_a);
    auto& monitor_a = registry.ctx().get<PopulationMonitor>();
    EXPECT_TRUE(monitor_a.latest.below_min_viable);
    EXPECT_FALSE(monitor_a.latest.above_soft_capacity);
    EXPECT_FALSE(monitor_a.latest.above_hard_cap);

    spawn_creature(registry, true, false);
    spawn_creature(registry, true, false);
    spawn_creature(registry, true, false);

    SimulationContext ctx_b(registry, 1.0 / 60.0, 1.0);
    system.tick(ctx_b);
    auto& monitor_b = registry.ctx().get<PopulationMonitor>();
    EXPECT_FALSE(monitor_b.latest.below_min_viable);
    EXPECT_TRUE(monitor_b.latest.above_soft_capacity);
    EXPECT_FALSE(monitor_b.latest.above_hard_cap);

    spawn_creature(registry, true, false);
    spawn_creature(registry, true, false);

    SimulationContext ctx_c(registry, 1.0 / 60.0, 2.0);
    system.tick(ctx_c);
    auto& monitor_c = registry.ctx().get<PopulationMonitor>();
    EXPECT_TRUE(monitor_c.latest.above_hard_cap);
}
