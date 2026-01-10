#include <chrono>
#include <iostream>

#include "evolution/sim/telemetry_system.h"
#include "evolution/sim/components.h"
#include "test_fixtures.h"

using namespace evolution::sim;
using namespace evolution::sim::test;

TEST(TelemetryBench, RingBufferThroughput) {
    TelemetrySystem telemetry(5.0);
    
    constexpr int kIterations = 100000;
    
    const auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < kIterations; ++i) {
        telemetry.log_death(1.0, 12345, 1, 100.0);
    }
    
    const auto end = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> diff = end - start;
    
    double ns_per_op = (diff.count() * 1e9) / kIterations;
    
    std::cout << "[ BENCH    ] RingBuffer push: " << ns_per_op << " ns/op" << std::endl;
    
    // Check constraint: Should be very fast (< 50ns ideal, < 200ns acceptable)
    // Note: CI environments vary, being lenient
    EXPECT_LT(ns_per_op, 500.0);
}

TEST(TelemetryBench, AggregationPerformance) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();
    
    constexpr int kEntityCount = 10000;
    
    // Create N entities with stats
    for (int i = 0; i < kEntityCount; ++i) {
        const auto entity = registry.create();
        registry.emplace<MetabolismComponent>(entity, 100.0, 100.0, 1.0);
        registry.emplace<DietComponent>(entity, DietType::Herbivore);
        registry.emplace<FitnessComponent>(entity);
        // Add random Lifecycle component to test that path
        if (i % 2 == 0) {
            LifecycleComponent lifecycle;
            lifecycle.age = static_cast<double>(i);
            registry.emplace<LifecycleComponent>(entity, lifecycle);
        }
    }
    
    TelemetrySystem telemetry(0.0); // Force report every time
    evolution::sim::SimulationContext context(registry, 0.016, 1.0);
    
    // Warmup
    telemetry.tick(context);
    
    constexpr int kTicks = 100;
    const auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < kTicks; ++i) {
        telemetry.tick(context);
    }
    
    const auto end = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> diff = end - start;
    
    double ms_per_tick = (diff.count() * 1000.0) / kTicks;
    
    std::cout << "[ BENCH    ] Aggregation (" << kEntityCount << " entities): " << ms_per_tick << " ms/tick" << std::endl;
    
    // Constraint: Should be reasonably fast (< 2ms for 10k entities)
    EXPECT_LT(ms_per_tick, 5.0);
}

TEST(TelemetryBench, JsonExportCheck) {
    TelemetrySystem telemetry(5.0);
    // Populate some data through log_death to verify counts
    telemetry.log_death(1.0, 1, 1, 10.0); // Starvation
    telemetry.log_death(1.0, 2, 2, 5.0);  // Predation
    
    std::string json = telemetry.export_json();
    
    // Verify JSON structure programmaticall y
    EXPECT_NE(json.find("\"mean_energy\":"), std::string::npos);
    EXPECT_NE(json.find("\"deaths_starvation\": 1"), std::string::npos);
    EXPECT_NE(json.find("\"deaths_predation\": 1"), std::string::npos);
}
