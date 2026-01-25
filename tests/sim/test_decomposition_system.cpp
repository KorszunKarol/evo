#include <gtest/gtest.h>

#include "evolution/sim/components.h"
#include "evolution/sim/decomposition_system.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/simulation_context.h"

namespace evolution::sim {

TEST(DecompositionSystem, DecaysCorpseAndReturnsNutrients) {
    entt::registry registry;

    SoilConfig soil_config{};
    soil_config.width_cells = 4;
    soil_config.height_cells = 4;
    soil_config.cell_size = 1.0;
    soil_config.baseline_nutrient = 2.0F;
    soil_config.max_nutrient = 10.0F;
    registry.ctx().emplace<SoilGrid>(soil_config);

    const entt::entity corpse_entity = registry.create();
    TransformComponent transform{};
    transform.position = Vec3{1.2, 0.0, 2.4};
    registry.emplace<TransformComponent>(corpse_entity, transform);

    CorpseComponent corpse{};
    corpse.biomass = 8.0;
    corpse.decay_rate = 2.0; // units per second
    corpse.age = 0.0;
    corpse.toxicity = 0.0;
    corpse.edible = true;
    registry.emplace<CorpseComponent>(corpse_entity, corpse);

    DecompositionSystem system;
    SimulationContext context(registry, 0.5, 0.0); // dt = 0.5s

    const auto& soil = registry.ctx().get<SoilGrid>();
    const float before = soil.at(1, 2);

    system.tick(context);

    const auto& corpse_after = registry.get<CorpseComponent>(corpse_entity);
    const float after = soil.at(1, 2);

    const double expected_loss = 1.0; // decay_rate * dt = 2.0 * 0.5
    EXPECT_NEAR(corpse_after.biomass, 7.0, 1e-9);
    EXPECT_NEAR(after - before, expected_loss, 1e-6);

    const auto& stats = registry.ctx().get<DecompositionStatistics>();
    EXPECT_NEAR(stats.biomass_decayed_last_tick, expected_loss, 1e-9);
    EXPECT_NEAR(stats.nutrients_returned_last_tick, expected_loss, 1e-9);
}

}  // namespace evolution::sim
