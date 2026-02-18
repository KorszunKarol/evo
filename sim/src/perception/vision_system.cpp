#include "evolution/sim/perception/vision_system.h"

#include <algorithm>
#include <cmath>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/creature_spatial_index.h"
#include "evolution/sim/environment/environment.h"

namespace evolution::sim {

namespace {

constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] Vec3 normalize_xz(const Vec3& v, const Vec3& fallback = Vec3{1.0, 0.0, 0.0}) noexcept {
    const double len_sq = v.x * v.x + v.z * v.z;
    if (len_sq <= 1e-9) {
        return fallback;
    }
    const double inv_len = 1.0 / std::sqrt(len_sq);
    return Vec3{v.x * inv_len, 0.0, v.z * inv_len};
}

[[nodiscard]] double clamp01(double v) noexcept {
    return std::clamp(v, 0.0, 1.0);
}

[[nodiscard]] double hit_type_norm(VisionHitType type) noexcept {
    constexpr double denom = static_cast<double>(VisionHitType::Terrain);
    if (denom <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(type) / denom;
}

void try_set_hit(VisionRaySample& sample,
                 VisionHitType hit_type,
                 double distance,
                 double max_range,
                 double energy_fraction) {
    if (distance < 0.0 || distance > max_range) {
        return;
    }
    const double normalized_distance = distance / std::max(1e-6, max_range);
    if (normalized_distance >= sample.distance_normalized) {
        return;
    }
    sample.distance_normalized = clamp01(normalized_distance);
    sample.hit_type = hit_type;
    sample.hit_energy_fraction = clamp01(energy_fraction);
}

}  // namespace

void VisionSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const auto* plant_index = registry.ctx().find<PlantSpatialIndex>();
    const auto* creature_index = registry.ctx().find<CreatureSpatialIndex>();
    const auto* terrain = registry.ctx().find<Terrain>();

    auto view = registry.view<TransformComponent,
                              VisionComponent,
                              VisionResult,
                              HeadingComponent>();

    for (auto entity : view) {
        const auto& transform = view.get<TransformComponent>(entity);
        const auto& vision = view.get<VisionComponent>(entity);
        auto& result = view.get<VisionResult>(entity);
        auto& heading = view.get<HeadingComponent>(entity);

        if (const auto* kinematics = registry.try_get<KinematicsComponent>(entity)) {
            heading.forward = normalize_xz(kinematics->linear_velocity, heading.forward);
        }

        const std::uint32_t ray_count_u32 = std::clamp<std::uint32_t>(vision.num_rays, 1, 64);
        const std::size_t ray_count = static_cast<std::size_t>(ray_count_u32);
        const double max_range = std::max(0.1, vision.max_range);
        const double fov_radians = std::clamp(vision.fov_degrees, 5.0, 360.0) * (kPi / 180.0);
        const double step_angle = (ray_count > 1) ? (fov_radians / static_cast<double>(ray_count - 1)) : 0.0;
        const double start_angle = -0.5 * fov_radians;
        const double ray_half_width = (ray_count > 1) ? (step_angle * 0.6) : (fov_radians * 0.5);

        result.rays.assign(ray_count, VisionRaySample{});
        result.buffer.assign(ray_count * 3, 0.0);

        std::vector<Vec3> ray_dirs(ray_count);
        for (std::size_t i = 0; i < ray_count; ++i) {
            const double angle = start_angle + static_cast<double>(i) * step_angle;
            const double c = std::cos(angle);
            const double s = std::sin(angle);
            const Vec3 dir{
                heading.forward.x * c - heading.forward.z * s,
                0.0,
                heading.forward.x * s + heading.forward.z * c,
            };
            ray_dirs[i] = normalize_xz(dir);
        }

        const Vec3 eye{
            transform.position.x,
            transform.position.y + vision.eye_height_offset,
            transform.position.z,
        };

        // Terrain hit approximation in XZ by marching fixed steps.
        if (terrain != nullptr) {
            constexpr int kSteps = 16;
            for (std::size_t i = 0; i < ray_count; ++i) {
                for (int step = 1; step <= kSteps; ++step) {
                    const double dist = (max_range * static_cast<double>(step)) / static_cast<double>(kSteps);
                    const Vec3 sample_pos = eye + ray_dirs[i] * dist;
                    const double ground = terrain->height(sample_pos.x, sample_pos.z);
                    if (sample_pos.y <= ground + 0.05) {
                        try_set_hit(result.rays[i], VisionHitType::Terrain, dist, max_range, 0.0);
                        break;
                    }
                }
            }
        }

        if (plant_index != nullptr) {
            plant_index->for_each_in_radius(
                registry,
                transform.position,
                max_range,
                [&](entt::entity plant_entity, double distance_sq) {
                    const auto* plant = registry.try_get<PlantComponent>(plant_entity);
                    const auto* plant_transform = registry.try_get<TransformComponent>(plant_entity);
                    if (plant == nullptr || plant_transform == nullptr || !plant->alive) {
                        return;
                    }
                    const Vec3 to_target = plant_transform->position - transform.position;
                    const Vec3 dir = normalize_xz(to_target);
                    const double distance = std::sqrt(std::max(0.0, distance_sq));
                    const double energy_fraction = plant->energy / std::max(1.0, plant->max_energy);

                    for (std::size_t i = 0; i < ray_count; ++i) {
                        const double alignment = std::clamp(ray_dirs[i].x * dir.x + ray_dirs[i].z * dir.z,
                                                            -1.0,
                                                            1.0);
                        const double angle = std::acos(alignment);
                        if (angle <= ray_half_width) {
                            try_set_hit(result.rays[i],
                                        VisionHitType::Plant,
                                        std::max(0.0, distance - plant->radius),
                                        max_range,
                                        energy_fraction);
                        }
                    }
                });
        }

        if (creature_index != nullptr) {
            creature_index->for_each_in_radius(
                registry,
                transform.position,
                max_range,
                [&](entt::entity other, double distance_sq) {
                    if (other == entity) {
                        return;
                    }
                    const auto* other_transform = registry.try_get<TransformComponent>(other);
                    const auto* other_metabolism = registry.try_get<MetabolismComponent>(other);
                    if (other_transform == nullptr || other_metabolism == nullptr ||
                        other_metabolism->energy <= 0.0) {
                        return;
                    }

                    VisionHitType hit_type = VisionHitType::Herbivore;
                    if (registry.all_of<CarnivoreTag>(other)) {
                        hit_type = VisionHitType::Carnivore;
                    } else if (const auto* diet = registry.try_get<DietComponent>(other);
                               diet != nullptr && diet->type == DietType::Carnivore) {
                        hit_type = VisionHitType::Carnivore;
                    }

                    const Vec3 to_target = other_transform->position - transform.position;
                    const Vec3 dir = normalize_xz(to_target);
                    const double distance = std::sqrt(std::max(0.0, distance_sq));
                    const double energy_fraction =
                        other_metabolism->energy / std::max(1.0, other_metabolism->max_energy);

                    for (std::size_t i = 0; i < ray_count; ++i) {
                        const double alignment = std::clamp(ray_dirs[i].x * dir.x + ray_dirs[i].z * dir.z,
                                                            -1.0,
                                                            1.0);
                        const double angle = std::acos(alignment);
                        if (angle <= ray_half_width) {
                            try_set_hit(result.rays[i], hit_type, distance, max_range, energy_fraction);
                        }
                    }
                });
        }

        for (std::size_t i = 0; i < ray_count; ++i) {
            const std::size_t base = i * 3;
            result.buffer[base + 0] = clamp01(result.rays[i].distance_normalized);
            result.buffer[base + 1] = hit_type_norm(result.rays[i].hit_type);
            result.buffer[base + 2] = clamp01(result.rays[i].hit_energy_fraction);
        }
    }
}

}  // namespace evolution::sim
