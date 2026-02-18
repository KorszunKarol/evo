#pragma once

#include <cstdint>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace evolution::client {

struct CreatureRenderItem {
    entt::entity id{entt::null};
    glm::vec3 position{0.0F};
    glm::vec3 velocity{0.0F};
    float radius{0.6F};
    float energy_norm{0.0F};
    float speed_norm{0.0F};
    std::uint32_t species_id{0};
    bool is_carnivore{false};
};

struct PlantRenderItem {
    entt::entity id{entt::null};
    glm::vec3 position{0.0F};
    float radius{0.5F};
    float energy_norm{0.0F};
    std::uint8_t species_id{0};
};

struct RenderSnapshot {
    double sim_time{0.0};
    std::vector<CreatureRenderItem> creatures{};
    std::vector<PlantRenderItem> plants{};
};

RenderSnapshot build_render_snapshot(entt::registry& registry, double sim_time);

}  // namespace evolution::client
