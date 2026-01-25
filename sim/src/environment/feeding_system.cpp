#include "evolution/sim/environment/feeding_system.h"

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"
#include "evolution/sim/creature_spatial_index.h"
#include "evolution/sim/physics/backend.h"
#include "evolution/sim/telemetry_system.h"

namespace evolution::sim {

void FeedingSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto* spatial_index = registry.ctx().find<PlantSpatialIndex>();
    auto* stats = registry.ctx().find<FeedingStatistics>();
    if (stats != nullptr) {
        stats->energy_transferred_last_tick = 0.0;
    }

    const double dt = context.fixed_dt();

    // Decrement combat cooldowns each tick
    auto combat_view = registry.view<CombatComponent>();
    for (auto entity : combat_view) {
        auto& combat = combat_view.get<CombatComponent>(entity);
        if (combat.attack_timer > 0.0) {
            combat.attack_timer = std::max(0.0, combat.attack_timer - dt);
        }
    }

    // Rebuild creature spatial index for this tick
    // Note: In a larger system, this might be a persistent resource in the registry context
    CreatureSpatialIndex creature_index(5.0); // 5.0 cell size
    creature_index.rebuild(registry);

    int fed_count = 0;

    // PHASE 3: Newtonian Predation via Contact Events
    IPhysicsBackend** backend_ptr = registry.ctx().find<IPhysicsBackend*>();
    if (backend_ptr != nullptr && *backend_ptr != nullptr) {
        IPhysicsBackend* backend = *backend_ptr;
        for (const auto& event : backend->contact_events()) {
            // Only process new or staying contacts
            if (event.end) continue;

            entt::entity a = event.entity_a;
            entt::entity b = event.entity_b;
            if (a == entt::null || b == entt::null) continue;

            auto process_predation = [&](entt::entity attacker, entt::entity victim) {
                auto* diet = registry.try_get<DietComponent>(attacker);
                auto* actuation = registry.try_get<ActuationComponent>(attacker);
                auto* metabolism = registry.try_get<MetabolismComponent>(attacker);
                auto* intent = registry.try_get<FeedingIntent>(attacker);
                
                if (!diet || diet->type != DietType::Carnivore || !actuation || !metabolism) return;
                if (!actuation->attack) return;

                auto* combat = registry.try_get<CombatComponent>(attacker);
                if (combat && combat->attack_timer > 0.0) return;

                // Victim can be a living herbivore or a corpse
                if (auto* victim_metabolism = registry.try_get<MetabolismComponent>(victim)) {
                    // Hunting living prey
                    if (victim_metabolism->energy <= 0.0) return;
                    if (!registry.any_of<HerbivoreTag>(victim)) return;

                    double remaining_capacity = metabolism->max_energy - metabolism->energy;
                    
                    // MOMENTUM DAMAGE (Phase 3)
                    double momentum_bonus = 1.0;
                    if (auto* k_a = registry.try_get<KinematicsComponent>(attacker)) {
                        if (auto* k_b = registry.try_get<KinematicsComponent>(victim)) {
                            Vec3 rel_vel = k_a->linear_velocity - k_b->linear_velocity;
                            double speed = std::sqrt(rel_vel.x*rel_vel.x + rel_vel.y*rel_vel.y + rel_vel.z*rel_vel.z);
                            momentum_bonus = 1.0 + (speed * 0.2); // 20% bonus per m/s
                        }
                    }

                    const double base_rate = intent ? intent->rate : 25.0;
                    const double transferable = std::min({base_rate * dt * momentum_bonus,
                                                          remaining_capacity,
                                                          victim_metabolism->energy});
                    if (transferable > 0.0) {
                        metabolism->energy += transferable;
                        victim_metabolism->energy -= transferable;
                        fed_count++;

                        if (auto* stats = registry.ctx().find<FeedingStatistics>()) {
                            stats->energy_from_hunting += transferable;
                            stats->energy_transferred_last_tick += transferable;
                        }

                        if (combat) {
                            combat->attack_timer = combat->attack_cooldown;
                            combat->target = victim;
                            combat->damage_dealt += transferable;
                        }

                        if (auto* telem = registry.try_get<TelemetryComponent>(attacker)) {
                            telem->total_energy_gained += transferable;
                            ++telem->successful_feeds;
                            if (victim_metabolism->energy <= 0.0) ++telem->kill_count;
                        }

                        // Telemetry system logging
                        if (telemetry_ != nullptr) {
                            auto* atk_transform = registry.try_get<TransformComponent>(attacker);
                            float ax = atk_transform ? static_cast<float>(atk_transform->position.x) : 0.0f;
                            float az = atk_transform ? static_cast<float>(atk_transform->position.z) : 0.0f;
                            telemetry_->log_feeding(registry.ctx().find<double>() ? *registry.ctx().find<double>() : 0.0,
                                                    static_cast<std::uint64_t>(attacker),
                                                    static_cast<std::uint64_t>(victim),
                                                    transferable, ax, az);
                        }

                        if (victim_metabolism->energy <= 0.0) {
                            if (auto* v_telem = registry.try_get<TelemetryComponent>(victim)) {
                                v_telem->killed_by_predation = true;
                                v_telem->death_cause = DeathCause::Predation;
                            }
                        }
                    }
                } else if (auto* corpse = registry.try_get<CorpseComponent>(victim)) {
                    // Scavenging
                    if (!corpse->edible || corpse->biomass <= 0.0) return;

                    double remaining_capacity = metabolism->max_energy - metabolism->energy;
                    const double transferable = std::min({10.0 * dt, remaining_capacity, corpse->biomass});
                    if (transferable > 0.0) {
                        metabolism->energy += transferable;
                        corpse->biomass -= transferable;
                        fed_count++;

                        if (auto* stats = registry.ctx().find<FeedingStatistics>()) {
                            stats->energy_from_scavenging += transferable;
                            stats->energy_transferred_last_tick += transferable;
                        }

                        if (auto* telem = registry.try_get<TelemetryComponent>(attacker)) {
                            telem->total_energy_gained += transferable;
                            ++telem->successful_feeds;
                        }
                        
                        // Scavenging has a short pause
                        if (combat) combat->attack_timer = 0.2;
                    }
                }
            };

            process_predation(a, b);
            process_predation(b, a);
        }
    }

    // Keep Herbivore-Plant logic (Spatial Index)
    auto herb_view = registry.view<TransformComponent, MetabolismComponent, FeedingIntent, HerbivoreTag>();
    for (auto entity : herb_view) {
        if (spatial_index == nullptr) break;

        auto& transform = herb_view.get<TransformComponent>(entity);
        auto& metabolism = herb_view.get<MetabolismComponent>(entity);
        const auto& intent = herb_view.get<FeedingIntent>(entity);

        if (!intent.request_eat || metabolism.energy >= metabolism.max_energy) continue;

        double remaining_capacity = metabolism.max_energy - metabolism.energy;
        double remaining_transfer = intent.rate * dt;

        spatial_index->for_each_in_radius(registry, transform.position, intent.reach + 2.0,
            [&](entt::entity plant_entity, double distance_sq) {
                if (remaining_transfer <= 0.0 || remaining_capacity <= 0.0) return;
                auto* plant = registry.try_get<PlantComponent>(plant_entity);
                if (!plant || !plant->alive) return;

                const double distance = std::sqrt(std::max(distance_sq, 0.0));
                if (distance > intent.reach + plant->radius) return;

                const double transferable = std::min({remaining_transfer, remaining_capacity, plant->energy});
                if (transferable <= 0.0) return;

                metabolism.energy += transferable;
                plant->energy -= transferable;
                remaining_transfer -= transferable;
                remaining_capacity -= transferable;
                fed_count++;

                if (auto* stats = registry.ctx().find<FeedingStatistics>()) {
                    stats->energy_from_plants += transferable;
                    stats->energy_transferred_last_tick += transferable;
                }

                if (auto* telem = registry.try_get<TelemetryComponent>(entity)) {
                    telem->total_energy_gained += transferable;
                    ++telem->successful_feeds;
                }

                // Telemetry system logging for herbivore feeding
                if (telemetry_ != nullptr) {
                    float hx = static_cast<float>(transform.position.x);
                    float hz = static_cast<float>(transform.position.z);
                    telemetry_->log_feeding(0.0, // sim_time will be approximated
                                            static_cast<std::uint64_t>(entity),
                                            static_cast<std::uint64_t>(plant_entity),
                                            transferable, hx, hz);
                }
                
                if (plant->energy <= 0.0) {
                    plant->energy = 0.0;
                    plant->alive = false;
                    plant->time_since_depleted = 0.0;
                }
            });
    }

    if (fed_count > 0) {
        SPDLOG_TRACE("FeedingSystem: Fed {} times this tick", fed_count);
    }
}

}  // namespace evolution::sim
