#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include "evolution/sim/components.h"
#include "evolution/sim/fitness_update_system.h"
#include "evolution/sim/simulation_context.h"

namespace evolution::sim::test {

TEST(FitnessUpdateSystemTest, AccumulatesDeterministicMetrics) {
    entt::registry registry;
    const entt::entity entity = registry.create();

    MetabolismComponent metabolism{};
    metabolism.energy = 42.0;
    metabolism.max_energy = 100.0;
    metabolism.basal_rate = 0.0;
    registry.emplace<MetabolismComponent>(entity, metabolism);
    registry.emplace<FitnessComponent>(entity);

    const FitnessWeights weights{1.0, 0.5, 3.0};
    FitnessUpdateSystem system(weights);

    SimulationContext context_step1{registry, 0.1, 0.0};
    system.tick(context_step1);

    const auto& fitness_step1 = registry.get<FitnessComponent>(entity);
    EXPECT_NEAR(fitness_step1.age_seconds, 0.1, 1e-9);
    EXPECT_NEAR(fitness_step1.energy_int_accum, 4.2, 1e-9);
    EXPECT_NEAR(fitness_step1.last_fitness, 0.1 + 0.5 * 4.2, 1e-9);

    SimulationContext context_step2{registry, 0.1, 0.1};
    system.tick(context_step2);

    const auto& fitness_step2 = registry.get<FitnessComponent>(entity);
    EXPECT_NEAR(fitness_step2.age_seconds, 0.2, 1e-9);
    EXPECT_NEAR(fitness_step2.energy_int_accum, 8.4, 1e-9);
    EXPECT_NEAR(fitness_step2.last_fitness, 0.2 + 0.5 * 8.4, 1e-9);
}

}  // namespace evolution::sim::test




