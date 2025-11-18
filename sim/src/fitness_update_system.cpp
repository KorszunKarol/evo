#include "evolution/sim/fitness_update_system.h"

#include <entt/entt.hpp>

namespace evolution::sim {

FitnessUpdateSystem::FitnessUpdateSystem(FitnessWeights weights) noexcept
    : weights_(weights) {}

void FitnessUpdateSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const double dt = context.fixed_dt();

    if (dt <= 0.0) {
        return;
    }

    auto view = registry.view<FitnessComponent, MetabolismComponent>();
    view.each([&](FitnessComponent& fitness, const MetabolismComponent& metabolism) {
        fitness.age_seconds += dt;
        fitness.energy_int_accum += metabolism.energy * dt;

        const double offspring_term = static_cast<double>(fitness.offspring_count) * weights_.offspring_weight;
        fitness.last_fitness =
            weights_.age_weight * fitness.age_seconds +
            weights_.energy_weight * fitness.energy_int_accum +
            offspring_term;
    });
}

}  // namespace evolution::sim





