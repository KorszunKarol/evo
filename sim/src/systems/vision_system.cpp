#include "evolution/sim/vision_system.h"

#include <cmath>

#include "evolution/sim/components.h"

namespace evolution::sim {

namespace {

[[nodiscard]] double dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] Vec3 normalize(const Vec3& v) noexcept {
    const double len = std::sqrt(dot(v, v));
    if (len < 1e-9) {
        return Vec3{0.0, 0.0, 1.0};
    }
    return Vec3{v.x / len, v.y / len, v.z / len};
}

[[nodiscard]] VisionHitType classify_entity(entt::registry& registry, entt::entity entity) noexcept {
    if (entity == entt::null) {
        return VisionHitType::Terrain;
    }
    if (registry.all_of<PlantComponent>(entity)) {
        return VisionHitType::Plant;
    }
    if (auto* diet = registry.try_get<DietComponent>(entity)) {
        if (diet->type == DietType::Carnivore) return VisionHitType::Carnivore;
        if (diet->type == DietType::Herbivore) return VisionHitType::Herbivore;
    }
    if (registry.all_of<HerbivoreTag>(entity)) {
        return VisionHitType::Herbivore;
    }
    return VisionHitType::Unknown;
}

}  // namespace

VisionSystem::VisionSystem(const IPhysicsBackend& backend) noexcept : backend_(backend) {}

void VisionSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    auto view = registry.view<VisionComponent, TransformComponent, KinematicsComponent>();

    for (auto entity : view) {
        auto& vision = view.get<VisionComponent>(entity);
        if (!vision.enabled || vision.ray_count == 0) {
            continue;
        }

        const auto& transform = view.get<TransformComponent>(entity);
        const auto& kinematics = view.get<KinematicsComponent>(entity);

        const std::size_t count = static_cast<std::size_t>(vision.ray_count);
        vision.ray_distances.resize(count, 1.0f);
        vision.ray_hit_types.resize(count, VisionHitType::None);
        vision.ray_hit_entities.resize(count, entt::null);

        Vec3 heading = normalize(kinematics.linear_velocity);
        const double vel_mag = kinematics.linear_velocity.length();
        if (vel_mag < 0.1) {
            heading = Vec3{0.0, 0.0, 1.0};
        }

        const double fov = static_cast<double>(vision.fov_radians);
        const double half_fov = fov * 0.5;
        const double max_range = static_cast<double>(vision.max_range);

        for (std::size_t i = 0; i < count; ++i) {
            double angle_offset = 0.0;
            if (count > 1) {
                angle_offset = -half_fov + (fov * static_cast<double>(i) / static_cast<double>(count - 1));
            }

            const double cos_a = std::cos(angle_offset);
            const double sin_a = std::sin(angle_offset);
            Vec3 ray_dir{
                heading.x * cos_a - heading.z * sin_a,
                0.0,
                heading.x * sin_a + heading.z * cos_a
            };
            ray_dir = normalize(ray_dir);

            const auto hit = backend_.raycast(transform.position, ray_dir, max_range);
            if (hit.has_value()) {
                const Vec3 diff = hit->point - transform.position;
                const double dist = std::sqrt(dot(diff, diff));
                vision.ray_distances[i] = static_cast<float>(std::clamp(dist / max_range, 0.0, 1.0));
                vision.ray_hit_entities[i] = hit->entity_b;
                vision.ray_hit_types[i] = classify_entity(registry, hit->entity_b);
            } else {
                vision.ray_distances[i] = 1.0f;
                vision.ray_hit_types[i] = VisionHitType::None;
                vision.ray_hit_entities[i] = entt::null;
            }
        }
    }
}

}  // namespace evolution::sim
