#include <gtest/gtest.h>

#include "evolution/sim/adaptive_control_system.h"
#include "evolution/sim/population_monitor.h"
#include "evolution/sim/simulation_context.h"

TEST(AdaptiveControlSystem, LowPopulationIncreasesRescueBiasAndSeeding) {
    entt::registry registry;
    evolution::sim::PopulationMonitor monitor;
    monitor.latest.creature_count = 80;
    monitor.latest.herbivore_to_plant_ratio = 0.01;
    registry.ctx().emplace<evolution::sim::PopulationMonitor>(monitor);
    registry.ctx().emplace<evolution::sim::PopulationEventCounters>();

    evolution::sim::AdaptiveControlConfig cfg{};
    cfg.enabled = true;
    cfg.target_population_min = 300;
    cfg.target_population_max = 1000;
    cfg.target_herbivore_plant_ratio_min = 0.03;
    cfg.target_herbivore_plant_ratio_max = 0.25;
    cfg.max_control_step_per_sec = 0.2;

    evolution::sim::AdaptiveControlSystem system(cfg);
    evolution::sim::SimulationContext context(registry, 1.0, 0.0);
    system.tick(context);

    const auto& state = registry.ctx().get<evolution::sim::AdaptiveControlState>();
    EXPECT_GT(state.rescue_bias, 0.0);
    EXPECT_LT(state.cull_bias, 0.1);
    EXPECT_GT(state.plant_seeding_multiplier_override, 0.0);
}

TEST(AdaptiveControlSystem, HighPopulationIncreasesCullBias) {
    entt::registry registry;
    evolution::sim::PopulationMonitor monitor;
    monitor.latest.creature_count = 2200;
    monitor.latest.herbivore_to_plant_ratio = 0.45;
    registry.ctx().emplace<evolution::sim::PopulationMonitor>(monitor);
    registry.ctx().emplace<evolution::sim::PopulationEventCounters>();

    evolution::sim::AdaptiveControlConfig cfg{};
    cfg.enabled = true;
    cfg.target_population_min = 300;
    cfg.target_population_max = 1200;
    cfg.target_herbivore_plant_ratio_min = 0.03;
    cfg.target_herbivore_plant_ratio_max = 0.25;
    cfg.max_control_step_per_sec = 0.25;

    evolution::sim::AdaptiveControlSystem system(cfg);
    evolution::sim::SimulationContext context(registry, 1.0, 1.0);
    system.tick(context);

    const auto& state = registry.ctx().get<evolution::sim::AdaptiveControlState>();
    EXPECT_GT(state.cull_bias, 0.0);
    EXPECT_LT(state.rescue_bias, 0.0);
    EXPECT_LT(state.plant_seeding_multiplier_override, 1.0);
}
