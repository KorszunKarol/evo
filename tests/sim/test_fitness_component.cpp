#include "test_fixtures.h"

#include <cmath>

#include "evolution/sim/components.h"
#include "evolution/sim/fitness_update_system.h"
#include "evolution/sim/simulation_context.h"

using namespace evolution::sim::test;
using namespace evolution::sim;

TEST(FitnessComponent, InitialState) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const entt::entity entity = registry.create();
    FitnessComponent fitness{};
    fitness.age_seconds = 0.0;
    fitness.energy_int_accum = 0.0;
    fitness.offspring_count = 0;
    fitness.last_fitness = 0.0;
    registry.emplace<FitnessComponent>(entity, fitness);

    const auto& stored = registry.get<FitnessComponent>(entity);
    EXPECT_DOUBLE_EQ(stored.age_seconds, 0.0);
    EXPECT_DOUBLE_EQ(stored.energy_int_accum, 0.0);
    EXPECT_EQ(stored.offspring_count, 0);
    EXPECT_DOUBLE_EQ(stored.last_fitness, 0.0);
}

TEST(FitnessComponent, AgeAccumulation) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const entt::entity entity = registry.create();
    FitnessComponent fitness{};
    registry.emplace<FitnessComponent>(entity, fitness);
    registry.emplace<LifecycleComponent>(entity);
    
    // FitnessUpdateSystem requires MetabolismComponent
    MetabolismComponent metabolism{};
    registry.emplace<MetabolismComponent>(entity, metabolism);

    FitnessWeights weights{1.0, 1.0, 1.0};  // age_weight=1.0, energy_weight=1.0, offspring_weight=1.0
    FitnessUpdateSystem system(weights);

    constexpr double dt = 0.016;  // ~60Hz

    // Run 100 ticks
    for (int i = 0; i < 100; ++i) {
        SimulationContext context(registry, dt, static_cast<double>(i) * dt);
        system.tick(context);
    }

    const auto& updated = registry.get<FitnessComponent>(entity);
    EXPECT_NEAR(updated.age_seconds, 100.0 * dt, 1e-6);
}

TEST(FitnessComponent, EnergyIntegralAccumulation) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const entt::entity entity = registry.create();
    FitnessComponent fitness{};
    registry.emplace<FitnessComponent>(entity, fitness);

    MetabolismComponent metabolism{};
    metabolism.energy = 50.0;
    metabolism.max_energy = 100.0;
    registry.emplace<MetabolismComponent>(entity, metabolism);

    FitnessWeights weights{0.0, 1.0, 0.0};  // Only energy weight
    FitnessUpdateSystem system(weights);

    constexpr double dt = 0.016;

    // Run 10 ticks with constant energy
    for (int i = 0; i < 10; ++i) {
        SimulationContext context(registry, dt, static_cast<double>(i) * dt);
        system.tick(context);
    }

    const auto& updated = registry.get<FitnessComponent>(entity);
    // Energy integral should accumulate: ∫energy·dt = energy * total_time
    EXPECT_NEAR(updated.energy_int_accum, 50.0 * 10.0 * dt, 1e-3);
}

TEST(FitnessComponent, EnergyIntegralWithChangingEnergy) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const entt::entity entity = registry.create();
    FitnessComponent fitness{};
    registry.emplace<FitnessComponent>(entity, fitness);

    MetabolismComponent metabolism{};
    metabolism.energy = 100.0;
    metabolism.max_energy = 100.0;
    registry.emplace<MetabolismComponent>(entity, metabolism);

    FitnessWeights weights{0.0, 1.0, 0.0};
    FitnessUpdateSystem system(weights);

    constexpr double dt = 0.016;
    constexpr int ticks = 10;

    // Energy decreases linearly
    for (int i = 0; i < ticks; ++i) {
        auto& metab = registry.get<MetabolismComponent>(entity);
        metab.energy = 100.0 - static_cast<double>(i) * 5.0;  // 100, 95, 90, ...

        SimulationContext context(registry, dt, static_cast<double>(i) * dt);
        system.tick(context);
    }

    const auto& updated = registry.get<FitnessComponent>(entity);
    // Rectangle rule: ∫energy ≈ dt * sum(energy[i]) where energy[i] is set before tick i
    double expected = 0.0;
    for (int i = 0; i < ticks; ++i) {
        const double energy = 100.0 - static_cast<double>(i) * 5.0;
        expected += dt * energy;
    }
    EXPECT_NEAR(updated.energy_int_accum, expected, 1e-2);
}

TEST(FitnessComponent, OffspringCountIncrement) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const entt::entity entity = registry.create();
    FitnessComponent fitness{};
    fitness.offspring_count = 0;
    registry.emplace<FitnessComponent>(entity, fitness);

    auto& stored = registry.get<FitnessComponent>(entity);
    EXPECT_EQ(stored.offspring_count, 0);

    stored.offspring_count = 5;
    EXPECT_EQ(stored.offspring_count, 5);
}

TEST(FitnessComponent, FitnessCalculation) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const entt::entity entity = registry.create();
    FitnessComponent fitness{};
    fitness.age_seconds = 10.0;
    fitness.energy_int_accum = 500.0;
    fitness.offspring_count = 3;
    registry.emplace<FitnessComponent>(entity, fitness);
    
    // FitnessUpdateSystem requires MetabolismComponent
    MetabolismComponent metabolism{};
    metabolism.energy = 0.0;  // Explicitly set to 0 (default is 100.0)
    registry.emplace<MetabolismComponent>(entity, metabolism);

    FitnessWeights weights{0.1, 0.001, 10.0};  // age=0.1, energy=0.001, offspring=10.0
    FitnessUpdateSystem system(weights);

    SimulationContext context(registry, 0.016, 0.0);
    system.tick(context);

    const auto& updated = registry.get<FitnessComponent>(entity);
    // After tick: age_seconds = 10.0 + dt, energy_int_accum = 500.0 + metabolism.energy * dt
    // Since metabolism.energy = 0.0, energy_int_accum stays 500.0
    const double expected_age = 10.0 + 0.016;
    const double expected = 0.1 * expected_age + 0.001 * 500.0 + 10.0 * 3.0;
    EXPECT_NEAR(updated.last_fitness, expected, 1e-6);
}

TEST(FitnessComponent, ZeroDtNoChange) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const entt::entity entity = registry.create();
    FitnessComponent fitness{};
    fitness.age_seconds = 5.0;
    fitness.energy_int_accum = 100.0;
    registry.emplace<FitnessComponent>(entity, fitness);

    MetabolismComponent metabolism{};
    metabolism.energy = 50.0;
    registry.emplace<MetabolismComponent>(entity, metabolism);

    FitnessWeights weights{1.0, 1.0, 1.0};
    FitnessUpdateSystem system(weights);

    SimulationContext context(registry, 0.0, 0.0);  // dt = 0
    system.tick(context);

    const auto& updated = registry.get<FitnessComponent>(entity);
    EXPECT_DOUBLE_EQ(updated.age_seconds, 5.0);
    EXPECT_DOUBLE_EQ(updated.energy_int_accum, 100.0);
}

TEST(FitnessComponent, MissingMetabolismHandled) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const entt::entity entity = registry.create();
    FitnessComponent fitness{};
    registry.emplace<FitnessComponent>(entity, fitness);
    // No MetabolismComponent

    FitnessWeights weights{1.0, 1.0, 1.0};
    FitnessUpdateSystem system(weights);

    SimulationContext context(registry, 0.016, 0.0);
    EXPECT_NO_THROW(system.tick(context));  // Should not crash

    const auto& updated = registry.get<FitnessComponent>(entity);
    EXPECT_DOUBLE_EQ(updated.energy_int_accum, 0.0);  // No energy to accumulate
}

TEST(FitnessComponent, MultipleEntitiesIndependent) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    std::vector<entt::entity> entities;
    for (int i = 0; i < 5; ++i) {
        const entt::entity entity = registry.create();
        FitnessComponent fitness{};
        fitness.age_seconds = static_cast<double>(i);
        registry.emplace<FitnessComponent>(entity, fitness);

        MetabolismComponent metabolism{};
        metabolism.energy = static_cast<double>(i) * 10.0;
        registry.emplace<MetabolismComponent>(entity, metabolism);

        entities.push_back(entity);
    }

    FitnessWeights weights{1.0, 1.0, 1.0};
    FitnessUpdateSystem system(weights);

    constexpr double dt = 0.016;
    SimulationContext context(registry, dt, 0.0);
    system.tick(context);

    for (std::size_t i = 0; i < entities.size(); ++i) {
        const auto& updated = registry.get<FitnessComponent>(entities[i]);
        EXPECT_NEAR(updated.age_seconds, static_cast<double>(i) + dt, 1e-6);
    }
}

TEST(FitnessComponent, NegativeEnergyHandled) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const entt::entity entity = registry.create();
    FitnessComponent fitness{};
    registry.emplace<FitnessComponent>(entity, fitness);

    MetabolismComponent metabolism{};
    metabolism.energy = -10.0;  // Negative energy
    registry.emplace<MetabolismComponent>(entity, metabolism);

    FitnessWeights weights{0.0, 1.0, 0.0};
    FitnessUpdateSystem system(weights);

    constexpr double dt = 0.016;
    SimulationContext context(registry, dt, 0.0);
    system.tick(context);

    const auto& updated = registry.get<FitnessComponent>(entity);
    // Should accumulate negative energy (or clamp to 0, depending on implementation)
    EXPECT_LE(updated.energy_int_accum, 0.0);
}

