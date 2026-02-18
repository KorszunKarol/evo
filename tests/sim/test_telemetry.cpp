#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/simulation_context.h"
#include "evolution/sim/telemetry_system.h"

namespace {

std::filesystem::path make_temp_dir() {
    const auto base = std::filesystem::temp_directory_path();
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return base / ("telemetry_test_" + unique);
}

}  // namespace

TEST(TelemetrySystem, WritesEventsJsonl) {
    const auto temp_dir = make_temp_dir();
    std::filesystem::create_directories(temp_dir);

    evolution::sim::TelemetrySystem telemetry(temp_dir, "test_run");
    evolution::sim::TelemetryEvent event{
        evolution::sim::TelemetryEventType::ENTITY_SPAWN,
        1.5,
        "{\"entity_id\":1}"
    };
    telemetry.emit_event(event, true);
    telemetry.flush();

    const auto events_path = temp_dir / "telemetry" / "events.jsonl";
    std::ifstream in(events_path);
    ASSERT_TRUE(in.good());

    std::string line;
    std::getline(in, line);
    EXPECT_NE(line.find("\"type\":\"ENTITY_SPAWN\""), std::string::npos);
    EXPECT_NE(line.find("\"run_id\":\"test_run\""), std::string::npos);

    std::filesystem::remove_all(temp_dir);
}

TEST(TelemetrySystem, WritesMovementMetrics) {
    const auto temp_dir = make_temp_dir();
    std::filesystem::create_directories(temp_dir);

    evolution::sim::TelemetryTargeting targeting{};
    targeting.sampling_rate = 1.0;
    evolution::sim::RollupConfig rollup{};
    rollup.interval_seconds = 0.0;
    rollup.buffer_size = 10;
    rollup.movement_capture_mode = evolution::sim::MovementCaptureMode::Sampled;

    entt::registry registry;
    const entt::entity entity = registry.create();
    targeting.target_entities.insert(entity);
    evolution::sim::TelemetrySystem telemetry(temp_dir, "test_run", targeting, rollup);
    registry.emplace<evolution::sim::TransformComponent>(entity, evolution::sim::TransformComponent{.position = {0.0, 0.0, 0.0}});
    registry.emplace<evolution::sim::KinematicsComponent>(entity, evolution::sim::KinematicsComponent{});

    evolution::sim::SimulationContext ctx1(registry, 1.0, 0.0);
    telemetry.tick(ctx1);

    auto& transform = registry.get<evolution::sim::TransformComponent>(entity);
    transform.position = evolution::sim::Vec3{1.0, 0.0, 0.0};

    evolution::sim::SimulationContext ctx2(registry, 1.0, 1.0);
    telemetry.tick(ctx2);
    telemetry.flush();

    const auto events_path = temp_dir / "telemetry" / "events.jsonl";
    std::ifstream in(events_path);
    ASSERT_TRUE(in.good());

    std::string line;
    bool found_movement = false;
    while (std::getline(in, line)) {
        if (line.find("\"type\":\"MOVEMENT_METRIC\"") != std::string::npos) {
            found_movement = true;
            break;
        }
    }

    EXPECT_TRUE(found_movement);
    std::filesystem::remove_all(temp_dir);
}

TEST(TelemetrySystem, EnforcesPerSecondBudgets) {
    const auto temp_dir = make_temp_dir();
    std::filesystem::create_directories(temp_dir);

    evolution::sim::TelemetryTargeting targeting{};
    targeting.sampling_rate = 1.0;
    evolution::sim::RollupConfig rollup{};
    rollup.interval_seconds = 0.0;
    rollup.buffer_size = 10;
    rollup.max_events_per_second = 2;
    rollup.max_events_per_type_per_second = 1;

    evolution::sim::TelemetrySystem telemetry(temp_dir, "budget_test", targeting, rollup);
    evolution::sim::TelemetryEvent a{evolution::sim::TelemetryEventType::ENTITY_SPAWN, 0.0, "{\"id\":1}"};
    evolution::sim::TelemetryEvent b{evolution::sim::TelemetryEventType::ENTITY_DEATH, 0.0, "{\"id\":2}"};

    EXPECT_TRUE(telemetry.emit_event(a, false));
    EXPECT_FALSE(telemetry.emit_event(a, false));  // per-type budget hit
    EXPECT_TRUE(telemetry.emit_event(b, false));
    EXPECT_FALSE(telemetry.emit_event(b, false));  // global budget hit

    telemetry.flush();

    const auto events_path = temp_dir / "telemetry" / "events.jsonl";
    std::ifstream in(events_path);
    ASSERT_TRUE(in.good());
    std::size_t lines = 0;
    std::string line;
    while (std::getline(in, line)) {
        ++lines;
    }
    EXPECT_EQ(lines, 2U);
    std::filesystem::remove_all(temp_dir);
}
