#include "evolution/sim/environment/feeding_system.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/creature_spatial_index.h"
#include "evolution/sim/population_monitor.h"

namespace evolution::sim {

namespace {

[[nodiscard]] bool is_valid_prey(entt::registry& registry,
                                 entt::entity predator,
                                 entt::entity prey) {
    if (prey == predator || !registry.valid(prey)) {
        return false;
    }
    if (!registry.all_of<TransformComponent, MetabolismComponent, HealthComponent>(prey)) {
        return false;
    }
    const auto& prey_health = registry.get<HealthComponent>(prey);
    if (prey_health.health <= 0.0) {
        return false;
    }
    if (registry.all_of<CarnivoreTag>(prey)) {
        return false;
    }
    if (const auto* prey_diet = registry.try_get<DietComponent>(prey);
        prey_diet != nullptr && prey_diet->type == DietType::Carnivore) {
        return false;
    }
    return true;
}

[[nodiscard]] double planar_distance_sq(const Vec3& a, const Vec3& b) noexcept {
    const double dx = a.x - b.x;
    const double dz = a.z - b.z;
    return dx * dx + dz * dz;
}

[[nodiscard]] Vec3 safe_direction(const Vec3& from, const Vec3& to) noexcept {
    Vec3 dir{to.x - from.x, 0.0, to.z - from.z};
    const double len_sq = dir.x * dir.x + dir.z * dir.z;
    if (len_sq <= 1e-9) {
        return Vec3{1.0, 0.0, 0.0};
    }
    const double inv_len = 1.0 / std::sqrt(len_sq);
    dir.x *= inv_len;
    dir.z *= inv_len;
    return dir;
}

}  // namespace

void FeedingSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto* plant_index = registry.ctx().find<PlantSpatialIndex>();
    auto* creature_index = registry.ctx().find<CreatureSpatialIndex>();
    auto* stats = registry.ctx().find<FeedingStatistics>();

    if (stats != nullptr) {
        stats->energy_transferred_last_tick = 0.0;
    }

    const double dt = context.fixed_dt();

    // Clear stale threat markers.
    {
        auto threat_view = registry.view<ThreatComponent>();
        std::vector<entt::entity> stale_threats;
        for (auto prey : threat_view) {
            const auto& threat = threat_view.get<ThreatComponent>(prey);
            if (threat.attacker_entity == entt::null || !registry.valid(threat.attacker_entity)) {
                stale_threats.push_back(prey);
            }
        }
        for (const entt::entity prey : stale_threats) {
            registry.remove<ThreatComponent>(prey);
        }
    }

    auto view = registry.view<TransformComponent, MetabolismComponent, FeedingIntent, DietComponent>();

    std::vector<entt::entity> to_destroy;
    int fed_count = 0;
    std::size_t actor_count = 0;
    std::size_t hungry_count = 0;
    std::size_t herbivore_actor_count = 0;
    std::size_t carnivore_actor_count = 0;
    std::size_t plant_candidates_seen = 0;
    std::size_t plant_candidates_in_reach = 0;
    std::size_t non_finite_position_count = 0;
    double herbivore_nearest_plant_sum = 0.0;
    std::size_t herbivore_nearest_plant_samples = 0;
    std::size_t herbivore_nearest_plant_miss = 0;
    double herbivore_min_x = std::numeric_limits<double>::infinity();
    double herbivore_max_x = -std::numeric_limits<double>::infinity();
    double herbivore_min_z = std::numeric_limits<double>::infinity();
    double herbivore_max_z = -std::numeric_limits<double>::infinity();

    for (auto entity : view) {
        ++actor_count;
        auto& transform = view.get<TransformComponent>(entity);
        auto& metabolism = view.get<MetabolismComponent>(entity);
        const auto& intent = view.get<FeedingIntent>(entity);
        const auto& diet = view.get<DietComponent>(entity);
        if (diet.type == DietType::Herbivore) {
            ++herbivore_actor_count;
        } else if (diet.type == DietType::Carnivore) {
            ++carnivore_actor_count;
        }

        if (!std::isfinite(transform.position.x) || !std::isfinite(transform.position.z)) {
            ++non_finite_position_count;
            continue;
        }

        if (!intent.request_eat || metabolism.energy >= metabolism.max_energy) {
            continue;
        }
        ++hungry_count;

        double remaining_capacity = std::max(0.0, metabolism.max_energy - metabolism.energy);
        double remaining_transfer = std::max(0.0, intent.rate * dt);
        if (remaining_capacity <= 0.0 || remaining_transfer <= 0.0) {
            continue;
        }

        if (diet.type == DietType::Herbivore) {
            herbivore_min_x = std::min(herbivore_min_x, transform.position.x);
            herbivore_max_x = std::max(herbivore_max_x, transform.position.x);
            herbivore_min_z = std::min(herbivore_min_z, transform.position.z);
            herbivore_max_z = std::max(herbivore_max_z, transform.position.z);
            if (plant_index != nullptr) {
                constexpr double kDiagnosticRadius = 128.0;
                double nearest_dist_sq = std::numeric_limits<double>::infinity();
                plant_index->for_each_in_radius(
                    registry,
                    transform.position,
                    kDiagnosticRadius,
                    [&](entt::entity, double distance_sq) {
                        nearest_dist_sq = std::min(nearest_dist_sq, distance_sq);
                    });
                if (std::isfinite(nearest_dist_sq)) {
                    herbivore_nearest_plant_sum += std::sqrt(std::max(nearest_dist_sq, 0.0));
                    ++herbivore_nearest_plant_samples;
                } else {
                    ++herbivore_nearest_plant_miss;
                }
            }
            if (plant_index == nullptr) {
                continue;
            }
            const double search_radius = intent.reach + 2.0;

            plant_index->for_each_in_radius(
                registry,
                transform.position,
                search_radius,
                [&](entt::entity plant_entity, double distance_sq) {
                    ++plant_candidates_seen;
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
                    ++plant_candidates_in_reach;

                    const double transferable = std::min({remaining_transfer, remaining_capacity, plant->energy});
                    if (transferable <= 0.0) {
                        return;
                    }

                    metabolism.energy += transferable;
                    plant->energy -= transferable;
                    remaining_transfer -= transferable;
                    remaining_capacity -= transferable;
                    ++fed_count;

                    if (stats != nullptr) {
                        stats->energy_transferred_last_tick += transferable;
                    }

                    if (plant->energy <= 0.0) {
                        plant->energy = 0.0;
                        plant->alive = false;
                        plant->time_since_depleted = 0.0;
                    }
                });
            continue;
        }

        if (diet.type != DietType::Carnivore || creature_index == nullptr) {
            continue;
        }

        auto* actuation = registry.try_get<ActuationComponent>(entity);
        auto* combat = registry.try_get<CombatComponent>(entity);
        auto* pursuit = registry.try_get<PursuitComponent>(entity);
        if (actuation == nullptr || combat == nullptr || pursuit == nullptr) {
            continue;
        }

        if (combat->attack_timer > 0.0) {
            combat->attack_timer = std::max(0.0, combat->attack_timer - dt);
        }

        auto disengage = [&]() {
            if (registry.valid(pursuit->target_entity)) {
                registry.remove<ThreatComponent>(pursuit->target_entity);
            }
            pursuit->target_entity = entt::null;
            pursuit->pursuit_time = 0.0;
            combat->target = entt::null;
        };

        const double engage_distance = std::max(0.1, pursuit->engage_distance);
        const double engage_distance_sq = engage_distance * engage_distance;

        bool has_target = is_valid_prey(registry, entity, pursuit->target_entity);
        if (has_target) {
            const auto& prey_transform = registry.get<TransformComponent>(pursuit->target_entity);
            if (planar_distance_sq(transform.position, prey_transform.position) > engage_distance_sq) {
                has_target = false;
            }
        }

        if (!has_target) {
            entt::entity nearest = entt::null;
            double nearest_dist_sq = std::numeric_limits<double>::max();
            creature_index->for_each_in_radius(
                registry,
                transform.position,
                engage_distance,
                [&](entt::entity prey_entity, double distance_sq) {
                    if (!is_valid_prey(registry, entity, prey_entity)) {
                        return;
                    }
                    if (distance_sq < nearest_dist_sq) {
                        nearest_dist_sq = distance_sq;
                        nearest = prey_entity;
                    }
                });

            if (nearest == entt::null) {
                disengage();
                continue;
            }

            pursuit->target_entity = nearest;
            pursuit->pursuit_time = 0.0;
            combat->target = nearest;
            has_target = true;
        }

        if (!has_target) {
            continue;
        }

        pursuit->pursuit_time += dt;
        const double pursuit_timeout_scale = std::clamp(tuning_.pursuit_timeout_scale, 0.2, 5.0);
        if (pursuit->pursuit_time > pursuit->max_pursuit_time * pursuit_timeout_scale) {
            disengage();
            continue;
        }

        const entt::entity prey = pursuit->target_entity;
        auto& prey_transform = registry.get<TransformComponent>(prey);
        auto& prey_metabolism = registry.get<MetabolismComponent>(prey);
        auto& prey_health = registry.get<HealthComponent>(prey);

        const Vec3 threat_dir = safe_direction(prey_transform.position, transform.position);
        registry.emplace_or_replace<ThreatComponent>(
            prey,
            ThreatComponent{.attacker_entity = entity, .threat_direction = threat_dir});

        combat->target = prey;

        const double dist_sq = planar_distance_sq(transform.position, prey_transform.position);
        const double attack_reach = std::max(0.1, combat->attack_reach);
        const bool in_attack_range = dist_sq <= (attack_reach * attack_reach);

        if (!actuation->attack || !in_attack_range || combat->attack_timer > 0.0) {
            continue;
        }

        const double damage =
            std::max(0.0, combat->attack_power * std::clamp(tuning_.predation_damage_scale, 0.1, 5.0) * dt);
        if (damage <= 0.0) {
            continue;
        }

        prey_health.health -= damage;
        combat->damage_dealt += damage;
        combat->attack_timer =
            std::max(0.01, combat->attack_cooldown * std::clamp(tuning_.attack_cooldown_scale, 0.2, 5.0));

        if (prey_health.health > 0.0) {
            continue;
        }

        // Kill and energy transfer.
        const double harvestable = std::max(
            0.0,
            prey_metabolism.energy * combat->conversion_efficiency *
                std::clamp(tuning_.predation_conversion_efficiency_scale, 0.1, 5.0));
        const double transferable = std::min({remaining_transfer, remaining_capacity, harvestable});
        if (transferable > 0.0) {
            metabolism.energy += transferable;
            remaining_transfer -= transferable;
            remaining_capacity -= transferable;
            prey_metabolism.energy = std::max(0.0, prey_metabolism.energy - transferable);
            ++fed_count;
            if (stats != nullptr) {
                stats->energy_transferred_last_tick += transferable;
            }
        }

        registry.remove<ThreatComponent>(prey);
        to_destroy.push_back(prey);
        disengage();
    }

    std::sort(to_destroy.begin(), to_destroy.end());
    to_destroy.erase(std::unique(to_destroy.begin(), to_destroy.end()), to_destroy.end());
    for (const entt::entity dead : to_destroy) {
        if (registry.valid(dead)) {
            registry.destroy(dead);
            if (auto* counters = registry.ctx().find<PopulationEventCounters>()) {
                ++counters->deaths_total;
            }
        }
    }

    if (fed_count > 0) {
        spdlog::trace("FeedingSystem: Fed {} times this tick", fed_count);
    }

    if (!debug_enabled_) {
        return;
    }
    debug_accumulator_ += dt;
    if (debug_interval_s_ > 0.0 && debug_accumulator_ + 1e-9 < debug_interval_s_) {
        return;
    }
    if (debug_interval_s_ > 0.0) {
        debug_accumulator_ = std::fmod(debug_accumulator_, debug_interval_s_);
    } else {
        debug_accumulator_ = 0.0;
    }

    const double sim_time = context.simulation_time();
    {
        const double transferred = (stats != nullptr) ? stats->energy_transferred_last_tick : 0.0;
        const double avg_nearest_plant = herbivore_nearest_plant_samples > 0
                                             ? herbivore_nearest_plant_sum /
                                                   static_cast<double>(herbivore_nearest_plant_samples)
                                             : -1.0;
        spdlog::info("[feeding-debug t={:.2f}] actors={} herbivores={} carnivores={} non_finite_pos={} hungry={} fed_count={} transferred={:.6f} plant_seen={} plant_in_reach={} avg_nearest_plant={:.3f} nearest_miss={} herb_x=[{:.2f},{:.2f}] herb_z=[{:.2f},{:.2f}] plant_index={} creature_index={}",
                     sim_time,
                     actor_count,
                     herbivore_actor_count,
                     carnivore_actor_count,
                     non_finite_position_count,
                     hungry_count,
                     fed_count,
                     transferred,
                     plant_candidates_seen,
                     plant_candidates_in_reach,
                     avg_nearest_plant,
                     herbivore_nearest_plant_miss,
                     std::isfinite(herbivore_min_x) ? herbivore_min_x : -1.0,
                     std::isfinite(herbivore_max_x) ? herbivore_max_x : -1.0,
                     std::isfinite(herbivore_min_z) ? herbivore_min_z : -1.0,
                     std::isfinite(herbivore_max_z) ? herbivore_max_z : -1.0,
                     plant_index != nullptr ? "yes" : "no",
                     creature_index != nullptr ? "yes" : "no");
    }
}

}  // namespace evolution::sim
