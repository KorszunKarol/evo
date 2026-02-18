#include "renderer/scene_extractor.h"

#include <algorithm>
#include <cmath>

#include "evolution/sim/components.h"

namespace evolution::client {

namespace {

[[nodiscard]] glm::vec3 to_glm(const evolution::sim::Vec3& v) {
    return glm::vec3(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
}

}  // namespace

RenderSnapshot build_render_snapshot(entt::registry& registry, double sim_time) {
    using namespace evolution::sim;

    RenderSnapshot snapshot;
    snapshot.sim_time = sim_time;

    auto creatures = registry.view<TransformComponent, MetabolismComponent, GenomeHandleComponent, KinematicsComponent>();
    snapshot.creatures.reserve(creatures.size_hint());

    for (auto entity : creatures) {
        const auto& tr = creatures.get<TransformComponent>(entity);
        const auto& meta = creatures.get<MetabolismComponent>(entity);
        const auto& genome = creatures.get<GenomeHandleComponent>(entity);
        const auto& kin = creatures.get<KinematicsComponent>(entity);

        float radius = 0.6F;
        if (const auto* collider = registry.try_get<ColliderComponent>(entity)) {
            if (collider->type == ShapeType::Sphere) {
                radius = static_cast<float>(collider->sphere.radius);
            } else if (collider->type == ShapeType::CapsuleY) {
                radius = static_cast<float>(collider->capsule.radius + collider->capsule.half_height);
            } else {
                radius = static_cast<float>(std::max({collider->aabb.half_extents.x,
                                                      collider->aabb.half_extents.y,
                                                      collider->aabb.half_extents.z}));
            }
        }

        const bool is_carnivore = registry.all_of<CarnivoreTag>(entity);
        const float energy_norm = meta.max_energy > 1e-6 ? static_cast<float>(meta.energy / meta.max_energy) : 0.0F;
        const float speed = static_cast<float>(std::sqrt(kin.linear_velocity.x * kin.linear_velocity.x +
                                                         kin.linear_velocity.y * kin.linear_velocity.y +
                                                         kin.linear_velocity.z * kin.linear_velocity.z));
        const float speed_norm = std::clamp(speed / 8.0F, 0.0F, 1.0F);

        snapshot.creatures.push_back(CreatureRenderItem{
            .id = entity,
            .position = to_glm(tr.position),
            .velocity = to_glm(kin.linear_velocity),
            .radius = std::max(0.1F, radius),
            .energy_norm = std::clamp(energy_norm, 0.0F, 1.0F),
            .speed_norm = speed_norm,
            .species_id = static_cast<std::uint32_t>(genome.id),
            .is_carnivore = is_carnivore,
        });
    }

    auto plants = registry.view<TransformComponent, PlantComponent>();
    snapshot.plants.reserve(plants.size_hint());
    for (auto entity : plants) {
        const auto& tr = plants.get<TransformComponent>(entity);
        const auto& plant = plants.get<PlantComponent>(entity);
        if (!plant.alive) {
            continue;
        }
        const float energy_norm = plant.max_energy > 1e-6 ? static_cast<float>(plant.energy / plant.max_energy) : 0.0F;
        snapshot.plants.push_back(PlantRenderItem{
            .id = entity,
            .position = to_glm(tr.position),
            .radius = static_cast<float>(std::max(0.08, plant.radius)),
            .energy_norm = std::clamp(energy_norm, 0.0F, 1.0F),
            .species_id = plant.species_id,
        });
    }

    return snapshot;
}

}  // namespace evolution::client
