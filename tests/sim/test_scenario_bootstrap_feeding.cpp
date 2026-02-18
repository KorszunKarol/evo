#include <algorithm>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/environment/feeding_system.h"
#include "evolution/sim/environment/plant_systems.h"
#include "evolution/sim/scenario.h"
#include "evolution/sim/simulation_app.h"

using namespace evolution::sim;

namespace {

struct PlantSample {
    Vec3 pos{};
    double radius{0.0};
};

double planar_distance_sq(const Vec3& a, const Vec3& b) {
    const double dx = a.x - b.x;
    const double dz = a.z - b.z;
    return dx * dx + dz * dz;
}

}  // namespace

TEST(ScenarioBootstrapFeeding, HerbivoresSpawnWithReachablePlants) {
    SimulationApp app;
    evolution::genetics::GenomeStorage storage;

    SimulationScenario scenario{};
    scenario.environment.terrain.width_cells = 512;
    scenario.environment.terrain.height_cells = 512;
    scenario.environment.terrain.cell_size = 2.0;
    scenario.environment.terrain.elevation_scale = 18.0;
    scenario.environment.terrain.base_frequency = 0.004;
    scenario.environment.terrain.seed = 2025;

    scenario.environment.soil.width_cells = 256;
    scenario.environment.soil.height_cells = 256;
    scenario.environment.soil.cell_size = 4.0;
    scenario.environment.soil.max_nutrient = 12.0F;
    scenario.environment.soil.baseline_nutrient = 5.0F;

    scenario.environment.plants.initial_count = 800;
    scenario.environment.plants.seed = 1337;
    scenario.initial_population = 24;
    scenario.genome_seed = 0xDEADBEEF;

    initialize_environment(app.registry(), scenario.environment);
    seed_initial_plants(app.registry(), scenario.environment);
    seed_initial_population(app.registry(), storage, scenario);

    auto& registry = app.registry();

    std::vector<PlantSample> plants;
    auto plant_view = registry.view<TransformComponent, PlantComponent>();
    plants.reserve(plant_view.size_hint());
    for (auto entity : plant_view) {
        const auto& plant = plant_view.get<PlantComponent>(entity);
        if (!plant.alive) {
            continue;
        }
        plants.push_back(PlantSample{
            .pos = plant_view.get<TransformComponent>(entity).position,
            .radius = plant.radius,
        });
    }

    ASSERT_FALSE(plants.empty());

    std::size_t herbivore_count = 0;
    std::size_t reachable_count = 0;
    auto creature_view =
        registry.view<TransformComponent, FeedingIntent, DietComponent, HerbivoreTag>();
    for (auto entity : creature_view) {
        const auto& transform = creature_view.get<TransformComponent>(entity);
        const auto& intent = creature_view.get<FeedingIntent>(entity);
        const auto& diet = creature_view.get<DietComponent>(entity);
        if (diet.type != DietType::Herbivore) {
            continue;
        }

        ++herbivore_count;
        const double reach_sq = (intent.reach + 1.0) * (intent.reach + 1.0);
        const bool found = std::any_of(plants.begin(),
                                       plants.end(),
                                       [&](const PlantSample& plant) {
                                           const double dist_sq = planar_distance_sq(transform.position, plant.pos);
                                           return dist_sq <= reach_sq;
                                       });
        if (found) {
            ++reachable_count;
        }
    }

    ASSERT_GT(herbivore_count, 0u);
    EXPECT_GE(reachable_count, herbivore_count / 2u);
}

TEST(ScenarioBootstrapFeeding, FeedingTransfersEnergyAfterBootstrap) {
    SimulationApp app;
    evolution::genetics::GenomeStorage storage;

    SimulationScenario scenario{};
    scenario.environment.terrain.width_cells = 512;
    scenario.environment.terrain.height_cells = 512;
    scenario.environment.terrain.cell_size = 2.0;
    scenario.environment.terrain.elevation_scale = 18.0;
    scenario.environment.terrain.base_frequency = 0.004;
    scenario.environment.terrain.seed = 2025;

    scenario.environment.soil.width_cells = 256;
    scenario.environment.soil.height_cells = 256;
    scenario.environment.soil.cell_size = 4.0;
    scenario.environment.soil.max_nutrient = 12.0F;
    scenario.environment.soil.baseline_nutrient = 5.0F;

    scenario.environment.plants.initial_count = 800;
    scenario.environment.plants.seed = 1337;
    scenario.initial_population = 24;
    scenario.genome_seed = 0xDEADBEEF;

    initialize_environment(app.registry(), scenario.environment);
    seed_initial_plants(app.registry(), scenario.environment);
    seed_initial_population(app.registry(), storage, scenario);

    auto& registry = app.registry();
    if (auto* index = registry.ctx().find<PlantSpatialIndex>()) {
        index->rebuild(registry);
    }

    auto hungry_view = registry.view<MetabolismComponent, DietComponent>();
    for (auto entity : hungry_view) {
        auto& metabolism = hungry_view.get<MetabolismComponent>(entity);
        const auto& diet = hungry_view.get<DietComponent>(entity);
        if (diet.type == DietType::Herbivore) {
            metabolism.energy = metabolism.max_energy * 0.5;
        }
    }

    FeedingSystem feeding;
    SimulationContext context(registry, 0.016666666666666666, 0.0);
    feeding.tick(context);

    const auto* stats = registry.ctx().find<FeedingStatistics>();
    ASSERT_NE(stats, nullptr);
    EXPECT_GT(stats->energy_transferred_last_tick, 0.0);
}
