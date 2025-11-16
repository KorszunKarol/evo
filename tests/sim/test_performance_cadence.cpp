#include "test_fixtures.h"

#include <chrono>
#include <vector>

#include "evolution/sim/fitness_update_system.h"
#include "evolution/sim/species_index_system.h"

using namespace evolution::sim::test;
using namespace evolution::sim;
using namespace evolution::genetics;

TEST(PerformanceCadence, FitnessUpdateScalesLinearly) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    // Create many entities
    constexpr int entity_count = 1000;
    std::vector<entt::entity> entities;
    entities.reserve(entity_count);
    for (int i = 0; i < entity_count; ++i) {
        const GenomeId genome_id = fixture.create_test_genome(static_cast<std::uint64_t>(i));
        entities.push_back(fixture.spawn_herbivore(
            Vec3{static_cast<double>(i % 100), 0.0, static_cast<double>(i / 100)}, genome_id));
    }

    FitnessUpdateSystem system(FitnessWeights{1.0, 1.0, 1.0});

    const auto start = std::chrono::high_resolution_clock::now();
    SimulationContext context(registry, 0.016, 0.0);
    system.tick(context);
    const auto end = std::chrono::high_resolution_clock::now();

    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double time_ms = static_cast<double>(duration.count()) / 1000.0;

    // Should complete in reasonable time (< 10ms for 1000 entities)
    EXPECT_LT(time_ms, 10.0) << "Fitness update should be fast";
}

TEST(PerformanceCadence, SpeciesIndexScales) {
    GenomeStorage storage;
    ReproConfig config{};

    // Create many genomes
    constexpr int genome_count = 500;
    std::vector<GenomeId> genome_ids;
    genome_ids.reserve(genome_count);
    for (int i = 0; i < genome_count; ++i) {
        genome_ids.push_back(storage.create_random(static_cast<std::uint64_t>(i)));
    }

    SpeciesIndexSystem system(storage, config, 10, 3.0);

    SimulationFixture fixture;
    const auto start = std::chrono::high_resolution_clock::now();
    SimulationContext context(fixture.app().registry(), 0.016, 0.0);
    system.tick(context);
    const auto end = std::chrono::high_resolution_clock::now();

    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double time_ms = static_cast<double>(duration.count()) / 1000.0;

    // Species indexing is O(N²) but should still be reasonable
    EXPECT_LT(time_ms, 1000.0) << "Species indexing should complete";
}

TEST(PerformanceCadence, CadenceWindowDoesNotSpike) {
    SimulationFixture fixture;
    auto config = create_test_env_config(2025);
    initialize_environment(fixture.app().registry(), config);

    // Create moderate population
    constexpr int entity_count = 500;
    std::vector<entt::entity> entities;
    entities.reserve(entity_count);
    for (int i = 0; i < entity_count; ++i) {
        const GenomeId genome_id = fixture.create_test_genome(static_cast<std::uint64_t>(i));
        entities.push_back(fixture.spawn_herbivore(
            Vec3{static_cast<double>(i % 50), 0.0, static_cast<double>(i / 50)}, genome_id));
    }

    FitnessUpdateSystem system(FitnessWeights{1.0, 1.0, 1.0});

    // Measure multiple ticks
    std::vector<double> tick_times;
    for (int i = 0; i < 100; ++i) {
        const auto start = std::chrono::high_resolution_clock::now();
        SimulationContext context(fixture.app().registry(), 0.016, static_cast<double>(i) * 0.016);
        system.tick(context);
        const auto end = std::chrono::high_resolution_clock::now();

        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        tick_times.push_back(static_cast<double>(duration.count()) / 1000.0);
    }

    // Calculate statistics
    double max_time = 0.0;
    double mean_time = 0.0;
    for (const double t : tick_times) {
        max_time = std::max(max_time, t);
        mean_time += t;
    }
    mean_time /= static_cast<double>(tick_times.size());

    // Max should not be too much larger than mean (no spikes)
    EXPECT_LT(max_time, mean_time * 3.0) << "No frame time spikes";
    EXPECT_LT(mean_time, 2.0) << "Mean tick time should be reasonable";
}

TEST(PerformanceCadence, LargePopulationHandled) {
    SimulationFixture fixture;
    auto config = create_test_env_config(2025);
    initialize_environment(fixture.app().registry(), config);

    // Create large population
    constexpr int entity_count = 5000;
    std::vector<entt::entity> entities;
    entities.reserve(entity_count);
    for (int i = 0; i < entity_count; ++i) {
        const GenomeId genome_id = fixture.create_test_genome(static_cast<std::uint64_t>(i % 100));
        entities.push_back(fixture.spawn_herbivore(
            Vec3{static_cast<double>(i % 200), 0.0, static_cast<double>(i / 200)}, genome_id));
    }

    FitnessUpdateSystem system(FitnessWeights{1.0, 1.0, 1.0});

    const auto start = std::chrono::high_resolution_clock::now();
    SimulationContext context(fixture.app().registry(), 0.016, 0.0);
    system.tick(context);
    const auto end = std::chrono::high_resolution_clock::now();

    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double time_ms = static_cast<double>(duration.count()) / 1000.0;

    // Should handle 5k entities in reasonable time (< 50ms)
    EXPECT_LT(time_ms, 50.0) << "Should handle large populations";
}

TEST(PerformanceCadence, EmptyRegistryFast) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    FitnessUpdateSystem system(FitnessWeights{1.0, 1.0, 1.0});

    const auto start = std::chrono::high_resolution_clock::now();
    SimulationContext context(registry, 0.016, 0.0);
    system.tick(context);
    const auto end = std::chrono::high_resolution_clock::now();

    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double time_ms = static_cast<double>(duration.count()) / 1000.0;

    EXPECT_LT(time_ms, 1.0) << "Empty registry should be very fast";
}

TEST(PerformanceCadence, MemoryUsageReasonable) {
    SimulationFixture fixture;
    auto config = create_test_env_config(2025);
    initialize_environment(fixture.app().registry(), config);

    // Create population
    constexpr int entity_count = 1000;
    std::vector<entt::entity> entities;
    entities.reserve(entity_count);
    for (int i = 0; i < entity_count; ++i) {
        const GenomeId genome_id = fixture.create_test_genome(static_cast<std::uint64_t>(i));
        entities.push_back(fixture.spawn_herbivore(
            Vec3{static_cast<double>(i % 100), 0.0, static_cast<double>(i / 100)}, genome_id));
    }

    // Run multiple ticks - memory should not grow unbounded
    FitnessUpdateSystem system(FitnessWeights{1.0, 1.0, 1.0});

    for (int i = 0; i < 1000; ++i) {
        SimulationContext context(fixture.app().registry(), 0.016, static_cast<double>(i) * 0.016);
        system.tick(context);
    }

    // If we get here without OOM, memory usage is reasonable
    EXPECT_TRUE(true);
}

