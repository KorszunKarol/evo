#include "evolution/sim/environment/feeding_system.h"

#include <algorithm>
#include <cmath>

#include "evolution/sim/components.h"

namespace evolution::sim {

void FeedingSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto* spatial_index = registry.ctx().find<PlantSpatialIndex>();
    if (spatial_index == nullptr) {
        return;
    }
    auto* stats = registry.ctx().find<FeedingStatistics>();
    if (stats != nullptr) {
        stats->energy_transferred_last_tick = 0.0;
    }

    const double dt = context.fixed_dt();

    auto view = registry.view<TransformComponent, MetabolismComponent, FeedingIntent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& metabolism = view.get<MetabolismComponent>(entity);
        const auto& intent = view.get<FeedingIntent>(entity);

        if (!intent.request_eat || metabolism.energy >= metabolism.max_energy) {
            continue;
        }

        double remaining_capacity = std::max(0.0, metabolism.max_energy - metabolism.energy);
        double remaining_transfer = intent.rate * dt;
        if (remaining_capacity <= 0.0 || remaining_transfer <= 0.0) {
            continue;
        }

        const double search_radius = intent.reach + 2.0;
        spatial_index->for_each_in_radius(
            registry,
            transform.position,
            search_radius,
            [&](entt::entity plant_entity, double distance_sq) {
                if (remaining_transfer <= 0.0 || remaining_capacity <= 0.0) {
                    return;
                }
                auto* plant = registry.try_get<PlantComponent>(plant_entity);
                auto* plant_transform = registry.try_get<TransformComponent>(plant_entity);
                if (plant == nullptr || plant_transform == nullptr || !plant->alive) {
                    return;
                }

                const double distance = std::sqrt(std::max(distance_sq, 0.0));
                if (distance > intent.reach + plant->radius) {
                    return;
                }

                const double transferable = std::min({remaining_transfer, remaining_capacity, plant->energy});
                if (transferable <= 0.0) {
                    return;
                }

                metabolism.energy += transferable;
                plant->energy -= transferable;
                remaining_transfer -= transferable;
                remaining_capacity -= transferable;
                if (stats != nullptr) {
                    stats->energy_transferred_last_tick += transferable;
                }

                if (plant->energy <= 0.0) {
                    plant->energy = 0.0;
                    plant->alive = false;
                    plant->time_since_depleted = 0.0;
                }
            });
    }
}

}  // namespace evolution::sim


