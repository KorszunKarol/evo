#include "evolution/sim/social_behavior_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace evolution::sim {

namespace {

constexpr double kEpsilon = 1e-6;

[[nodiscard]] Vec3 NormalizeXZ(const Vec3& value) noexcept {
    const double len = std::sqrt(value.x * value.x + value.z * value.z);
    if (len < kEpsilon) {
        return Vec3{0.0, 0.0, 0.0};
    }
    return Vec3{value.x / len, 0.0, value.z / len};
}

[[nodiscard]] double DistanceXZ(const Vec3& a, const Vec3& b) noexcept {
    const double dx = a.x - b.x;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

}  // namespace

void SocialBehaviorSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto* index = registry.ctx().find<CreatureSpatialIndex>();
    if (index == nullptr) {
        return;
    }

    if (scratch_neighbors_.capacity() < 64U) {
        scratch_neighbors_.reserve(64U);
    }

    auto view = registry.view<TransformComponent,
                              KinematicsComponent,
                              MetabolismComponent,
                              DietComponent>();

    for (auto entity : view) {
        const auto& metabolism = view.get<MetabolismComponent>(entity);
        if (metabolism.energy <= 0.0) {
            continue;
        }

        const auto& transform = view.get<TransformComponent>(entity);
        const auto& diet = view.get<DietComponent>(entity);

        auto& territory = registry.get_or_emplace<TerritoryComponent>(entity);
        if (!territory.initialized) {
            territory.center = transform.position;
            territory.radius = kTerritoryRadius;
            territory.initialized = true;
        }
        if (territory.radius <= 0.0) {
            territory.radius = kTerritoryRadius;
        }

        auto& signals = registry.get_or_emplace<SocialSignalsComponent>(entity);
        signals = SocialSignalsComponent{};

        scratch_neighbors_.clear();
        index->for_each_in_radius(registry, transform.position, kNeighborRadius,
                                  [&](entt::entity other) {
                                      scratch_neighbors_.push_back(other);
                                      return true;
                                  });
        std::sort(scratch_neighbors_.begin(), scratch_neighbors_.end());

        std::size_t neighbor_count = 0U;
        Vec3 sum_positions{0.0, 0.0, 0.0};
        Vec3 sum_velocities{0.0, 0.0, 0.0};
        Vec3 separation_sum{0.0, 0.0, 0.0};

        for (const auto other : scratch_neighbors_) {
            if (other == entity) {
                continue;
            }

            const auto* other_metabolism = registry.try_get<MetabolismComponent>(other);
            if (other_metabolism == nullptr || other_metabolism->energy <= 0.0) {
                continue;
            }

            const auto* other_diet = registry.try_get<DietComponent>(other);
            if (other_diet == nullptr || other_diet->type != diet.type) {
                continue;
            }

            const auto* other_transform = registry.try_get<TransformComponent>(other);
            const auto* other_kinematics = registry.try_get<KinematicsComponent>(other);
            if (other_transform == nullptr || other_kinematics == nullptr) {
                continue;
            }

            ++neighbor_count;
            sum_positions.x += other_transform->position.x;
            sum_positions.z += other_transform->position.z;
            sum_velocities.x += other_kinematics->linear_velocity.x;
            sum_velocities.z += other_kinematics->linear_velocity.z;

            const double dx = transform.position.x - other_transform->position.x;
            const double dz = transform.position.z - other_transform->position.z;
            const double dist = std::sqrt(dx * dx + dz * dz);
            if (dist <= kSeparationRadius && dist > kEpsilon) {
                const double inv = 1.0 / std::max(dist, kEpsilon);
                separation_sum.x += (dx * inv) * inv;
                separation_sum.z += (dz * inv) * inv;
            }
        }

        if (neighbor_count > 0U) {
            const double inv_count = 1.0 / static_cast<double>(neighbor_count);
            const Vec3 centroid{sum_positions.x * inv_count, 0.0, sum_positions.z * inv_count};
            const Vec3 cohesion{centroid.x - transform.position.x,
                                0.0,
                                centroid.z - transform.position.z};
            signals.cohesion_dir = NormalizeXZ(cohesion);
            const Vec3 mean_velocity{sum_velocities.x * inv_count, 0.0, sum_velocities.z * inv_count};
            signals.alignment_dir = NormalizeXZ(mean_velocity);
            signals.separation_dir = NormalizeXZ(separation_sum);
            signals.neighbor_density = std::clamp(
                static_cast<double>(neighbor_count) / kMaxNeighborsForDensity, 0.0, 1.0);
        }

        signals.territory_dist_norm = std::clamp(
            DistanceXZ(transform.position, territory.center) / territory.radius,
            0.0,
            1.0);

        scratch_neighbors_.clear();
        index->for_each_in_radius(registry, territory.center, territory.radius,
                                  [&](entt::entity other) {
                                      scratch_neighbors_.push_back(other);
                                      return true;
                                  });
        std::sort(scratch_neighbors_.begin(), scratch_neighbors_.end());

        std::size_t intruder_count = 0U;
        for (const auto other : scratch_neighbors_) {
            if (other == entity) {
                continue;
            }
            const auto* other_metabolism = registry.try_get<MetabolismComponent>(other);
            if (other_metabolism == nullptr || other_metabolism->energy <= 0.0) {
                continue;
            }
            const auto* other_diet = registry.try_get<DietComponent>(other);
            if (other_diet == nullptr || other_diet->type == diet.type) {
                continue;
            }
            ++intruder_count;
        }

        signals.intruder_density = std::clamp(
            static_cast<double>(intruder_count) / kMaxIntrudersForDensity, 0.0, 1.0);

        if (diet.type == DietType::Carnivore) {
            entt::entity best_prey = entt::null;
            double best_dist_sq = std::numeric_limits<double>::infinity();

            scratch_neighbors_.clear();
            index->for_each_in_radius(registry, transform.position, kMaxPreyRadius,
                                      [&](entt::entity other) {
                                          scratch_neighbors_.push_back(other);
                                          return true;
                                      });
            std::sort(scratch_neighbors_.begin(), scratch_neighbors_.end());

            for (const auto other : scratch_neighbors_) {
                if (other == entity) {
                    continue;
                }
                if (!registry.all_of<HerbivoreTag>(other)) {
                    continue;
                }
                const auto* other_metabolism = registry.try_get<MetabolismComponent>(other);
                const auto* other_transform = registry.try_get<TransformComponent>(other);
                if (other_metabolism == nullptr || other_transform == nullptr ||
                    other_metabolism->energy <= 0.0) {
                    continue;
                }

                const double dx = other_transform->position.x - transform.position.x;
                const double dz = other_transform->position.z - transform.position.z;
                const double dist_sq = dx * dx + dz * dz;
                if (dist_sq + kEpsilon < best_dist_sq) {
                    best_dist_sq = dist_sq;
                    best_prey = other;
                    continue;
                }
                if (std::abs(dist_sq - best_dist_sq) <= kEpsilon && other < best_prey) {
                    best_prey = other;
                }
            }

            if (best_prey != entt::null) {
                const auto& prey_transform = registry.get<TransformComponent>(best_prey);
                const Vec3 to_prey{prey_transform.position.x - transform.position.x,
                                   0.0,
                                   prey_transform.position.z - transform.position.z};
                signals.prey_dir = NormalizeXZ(to_prey);

                scratch_neighbors_.clear();
                index->for_each_in_radius(registry, prey_transform.position, kNearPreyRadius,
                                          [&](entt::entity other) {
                                              scratch_neighbors_.push_back(other);
                                              return true;
                                          });
                std::sort(scratch_neighbors_.begin(), scratch_neighbors_.end());

                std::size_t pack_count = 0U;
                for (const auto other : scratch_neighbors_) {
                    if (other == entity) {
                        continue;
                    }
                    if (!registry.all_of<CarnivoreTag>(other)) {
                        continue;
                    }
                    const auto* other_metabolism = registry.try_get<MetabolismComponent>(other);
                    if (other_metabolism == nullptr || other_metabolism->energy <= 0.0) {
                        continue;
                    }
                    ++pack_count;
                }

                signals.pack_density_near_prey = std::clamp(
                    static_cast<double>(pack_count) / kMaxPackForDensity, 0.0, 1.0);
            }
        }
    }
}

}  // namespace evolution::sim
