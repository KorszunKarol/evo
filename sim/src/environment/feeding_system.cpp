#include "evolution/sim/environment/feeding_system.h"

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"

namespace evolution::sim {

void FeedingSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto* spatial_index = registry.ctx().find<PlantSpatialIndex>();
    auto* stats = registry.ctx().find<FeedingStatistics>();
    if (stats != nullptr) {
        stats->energy_transferred_last_tick = 0.0;
    }

    const double dt = context.fixed_dt();

    auto view = registry.view<TransformComponent, MetabolismComponent, FeedingIntent, DietComponent>();
    auto prey_view = registry.view<TransformComponent, MetabolismComponent>();

    int fed_count = 0;
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& metabolism = view.get<MetabolismComponent>(entity);
        const auto& intent = view.get<FeedingIntent>(entity);
        const auto& diet = view.get<DietComponent>(entity);

        if (!intent.request_eat || metabolism.energy >= metabolism.max_energy) {
            continue;
        }

        double remaining_capacity = std::max(0.0, metabolism.max_energy - metabolism.energy);
        double remaining_transfer = intent.rate * dt;
        if (remaining_capacity <= 0.0 || remaining_transfer <= 0.0) {
            continue;
        }

        if (diet.type == DietType::Herbivore) {
            if (spatial_index == nullptr) {
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
                    fed_count++;
                    
                    if (stats != nullptr) {
                        stats->energy_transferred_last_tick += transferable;
                    }

                    if (plant->energy <= 0.0) {
                        plant->energy = 0.0;
                        plant->alive = false;
                        plant->time_since_depleted = 0.0;
                    }
                });
        } else if (diet.type == DietType::Carnivore) {
             // Check attack intent from brain
             auto* actuation = registry.try_get<ActuationComponent>(entity);
             if (actuation == nullptr || !actuation->attack) {
                 continue;  // Brain chose not to attack
             }
             
             // Check and update attack cooldown
             auto* combat = registry.try_get<CombatComponent>(entity);
             if (combat != nullptr) {
                 if (combat->attack_timer > 0.0) {
                     combat->attack_timer -= dt;
                     continue;  // Still in cooldown, cannot attack
                 }
             }
             
             // Simple O(N) scan for prey (other entities with metabolism)
             // Optimization: In future, use a spatial index for creatures.
             for (auto prey_entity : prey_view) {
                 if (remaining_transfer <= 0.0 || remaining_capacity <= 0.0) {
                     break;
                 }
                 if (prey_entity == entity) continue; // Don't eat self
                 
                 // Only attack herbivores (not other carnivores for now)
                 auto* prey_diet = registry.try_get<DietComponent>(prey_entity);
                 if (prey_diet != nullptr && prey_diet->type == DietType::Carnivore) {
                     continue;  // Don't attack other predators
                 }

                 auto& prey_transform = prey_view.get<TransformComponent>(prey_entity);
                 auto& prey_metabolism = prey_view.get<MetabolismComponent>(prey_entity);

                 if (prey_metabolism.energy <= 0.0) continue; // Skip dead prey

                 const double dx = transform.position.x - prey_transform.position.x;
                 const double dz = transform.position.z - prey_transform.position.z;
                 const double dist_sq = dx * dx + dz * dz;
                 
                 const double reach_sq = intent.reach * intent.reach;

                 if (dist_sq <= reach_sq) {
                     const double prey_available = prey_metabolism.energy;
                     const double transferable = std::min({remaining_transfer, remaining_capacity, prey_available});

                     if (transferable > 0.0) {
                         metabolism.energy += transferable;
                         prey_metabolism.energy -= transferable;
                         remaining_transfer -= transferable;
                         remaining_capacity -= transferable;
                         fed_count++;
                         
                         // Reset attack cooldown after successful attack
                          if (combat != nullptr) {
                              combat->attack_timer = combat->attack_cooldown;
                              combat->target = prey_entity;
                              combat->damage_dealt += transferable;
                          }
                          
                          if (stats != nullptr) {
                              stats->energy_transferred_last_tick += transferable;
                          }
                         
                         // Only attack one prey per tick
                         break;
                     }
                 }
             }
        }
    }
    if (fed_count > 0) {
        spdlog::trace("FeedingSystem: Fed {} times this tick", fed_count);
    }
}

}  // namespace evolution::sim
