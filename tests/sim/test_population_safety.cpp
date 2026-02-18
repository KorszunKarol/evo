#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/population_monitor.h"
#include "evolution/sim/simulation_context.h"
#include "evolution/sim/telemetry_system.h"
#include "test_fixtures.h"

using namespace evolution::sim;
using namespace evolution::sim::test;

namespace {

std::filesystem::path make_temp_dir() {
    const auto base = std::filesystem::temp_directory_path();
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return base / ("population_safety_test_" + unique);
}

class PopulationSafetyFixture : public SimulationFixture {
public:
    void Initialize() { SetUp(); }
};

}  // namespace

TEST(PopulationSafety, ExtinctionRescueSpawnsConfiguredBatch) {
    PopulationSafetyFixture fixture;
    fixture.Initialize();

    auto& registry = fixture.app().registry();

    PopulationConfig config{};
    config.min_viable_population = 5;
    config.extinction_grace_ticks = 2;
    config.rescue_batch_size = 4;
    config.hard_cap = 200;
    config.target_cap = 150;

    PopulationSystem system(fixture.storage(), config, 1234);

    for (int i = 0; i < 3; ++i) {
        SimulationContext ctx(registry, 1.0 / 60.0, i / 60.0);
        system.tick(ctx);
    }

    const auto& monitor = registry.ctx().get<PopulationMonitor>();
    const auto& counters = registry.ctx().get<PopulationEventCounters>();

    EXPECT_GE(monitor.latest.creature_count, 4u);
    EXPECT_EQ(counters.rescues_total, 1u);
    EXPECT_GE(counters.births_total, 4u);
}

TEST(PopulationSafety, OverpopulationCullRemovesLowestFitnessFirst) {
    PopulationSafetyFixture fixture;
    fixture.Initialize();

    auto& registry = fixture.app().registry();

    std::vector<entt::entity> entities;
    for (int i = 0; i < 6; ++i) {
        const entt::entity e = registry.create();
        registry.emplace<MetabolismComponent>(e, MetabolismComponent{});
        registry.emplace<FitnessComponent>(e, FitnessComponent{.last_fitness = static_cast<double>(i)});
        entities.push_back(e);
    }

    PopulationConfig config{};
    config.min_viable_population = 0;
    config.hard_cap = 5;
    config.target_cap = 3;
    config.extinction_grace_ticks = 100;

    PopulationSystem system(fixture.storage(), config, 5678);
    SimulationContext ctx(registry, 1.0 / 60.0, 0.0);
    system.tick(ctx);

    auto creatures = registry.view<MetabolismComponent>();
    EXPECT_EQ(creatures.size(), 3u);

    // Should keep highest fitness entities: 3,4,5
    int kept_high = 0;
    for (auto e : creatures) {
        const auto* fit = registry.try_get<FitnessComponent>(e);
        ASSERT_NE(fit, nullptr);
        if (fit->last_fitness >= 3.0) {
            ++kept_high;
        }
    }
    EXPECT_EQ(kept_high, 3);

    const auto& counters = registry.ctx().get<PopulationEventCounters>();
    EXPECT_EQ(counters.culls_total, 1u);
    EXPECT_EQ(counters.deaths_total, 3u);
}

TEST(PopulationSafety, RescueAndCullEventsAreTelemetryLogged) {
    PopulationSafetyFixture fixture;
    fixture.Initialize();

    auto& registry = fixture.app().registry();
    const auto temp_dir = make_temp_dir();
    std::filesystem::create_directories(temp_dir);

    TelemetryTargeting targeting{};
    targeting.sampling_rate = 1.0;
    RollupConfig rollup{};
    rollup.interval_seconds = 1000.0;

    TelemetrySystem telemetry(temp_dir, "pop_test", targeting, rollup);
    registry.ctx().emplace<TelemetryContext>(TelemetryContext{.system = &telemetry});

    PopulationConfig config{};
    config.min_viable_population = 3;
    config.extinction_grace_ticks = 1;
    config.rescue_batch_size = 2;
    config.hard_cap = 3;
    config.target_cap = 2;

    PopulationSystem system(fixture.storage(), config, 7777);

    // Trigger rescue
    SimulationContext ctx0(registry, 1.0 / 60.0, 0.0);
    system.tick(ctx0);

    // Inflate population to trigger cull
    for (int i = 0; i < 6; ++i) {
        const entt::entity e = registry.create();
        registry.emplace<MetabolismComponent>(e);
        registry.emplace<FitnessComponent>(e, FitnessComponent{.last_fitness = static_cast<double>(i)});
    }

    SimulationContext ctx1(registry, 1.0 / 60.0, 1.0);
    system.tick(ctx1);

    telemetry.flush();

    const auto events_path = temp_dir / "telemetry" / "events.jsonl";
    std::ifstream in(events_path);
    ASSERT_TRUE(in.good());

    bool saw_rescue = false;
    bool saw_cull = false;
    std::string line;
    while (std::getline(in, line)) {
        saw_rescue = saw_rescue || line.find("\"type\":\"POPULATION_RESCUE\"") != std::string::npos;
        saw_cull = saw_cull || line.find("\"type\":\"OVERPOPULATION_CULL\"") != std::string::npos;
    }

    EXPECT_TRUE(saw_rescue);
    EXPECT_TRUE(saw_cull);

    std::filesystem::remove_all(temp_dir);
}
