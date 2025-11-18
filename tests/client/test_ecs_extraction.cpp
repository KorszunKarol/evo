#include "test_render_fixtures.h"

#include <glm/glm.hpp>

#include "evolution/sim/components.h"

namespace evolution::client::test {

// Helper to convert Vec3 to glm::vec3
[[nodiscard]] glm::vec3 to_glm(const evolution::sim::Vec3& v) {
    return glm::vec3(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
}

TEST_F(RenderClientFixture, ECSHasTerrain) {
    setup_minimal_simulation();
    auto& reg = registry();

    const auto* terrain = reg.ctx().find<evolution::sim::Terrain>();
    ASSERT_NE(terrain, nullptr);

    EXPECT_GT(terrain->width(), 0);
    EXPECT_GT(terrain->height_cells(), 0);
    EXPECT_GT(terrain->cell_size(), 0.0);
}

TEST_F(RenderClientFixture, ECSHasBiomeMap) {
    setup_minimal_simulation();
    auto& reg = registry();

    const auto* biome_map = reg.ctx().find<evolution::sim::BiomeMap>();
    // Biome map may or may not exist depending on config
    if (biome_map != nullptr) {
        const double test_x = 50.0;
        const double test_z = 50.0;
        const auto biome_id = biome_map->sample(test_x, test_z);
        EXPECT_GE(static_cast<int>(biome_id), 0);
    }
}

TEST_F(RenderClientFixture, ECSHasWaterMap) {
    setup_minimal_simulation();
    auto& reg = registry();

    const auto* water_map = reg.ctx().find<evolution::sim::WaterMap>();
    // Water map may or may not exist depending on config
    if (water_map != nullptr) {
        const double water_level = water_map->water_level();
        EXPECT_FALSE(std::isnan(water_level));
        EXPECT_FALSE(std::isinf(water_level));
    }
}

TEST_F(RenderClientFixture, ECSAgentComponents) {
    setup_simulation_with_entities(5, 0);

    auto& reg = registry();
    auto view = reg.view<const evolution::sim::TransformComponent,
                         const evolution::sim::ColliderComponent,
                         const evolution::sim::MetabolismComponent,
                         const evolution::sim::LifecycleComponent,
                         const evolution::sim::HerbivoreTag>();

    std::size_t count = 0;
    for (auto entity : view) {
        ++count;
        const auto& transform = view.get<const evolution::sim::TransformComponent>(entity);
        const auto& collider = view.get<const evolution::sim::ColliderComponent>(entity);
        const auto& metabolism = view.get<const evolution::sim::MetabolismComponent>(entity);
        const auto& lifecycle = view.get<const evolution::sim::LifecycleComponent>(entity);

        // Verify components have valid data
        EXPECT_FALSE(std::isnan(transform.position.x));
        // Verify collider type is valid
        EXPECT_TRUE(collider.type == evolution::sim::ShapeType::Sphere ||
                    collider.type == evolution::sim::ShapeType::CapsuleY ||
                    collider.type == evolution::sim::ShapeType::Aabb);
        EXPECT_GT(metabolism.max_energy, 0.0);
        EXPECT_GE(metabolism.energy, 0.0);
        EXPECT_LE(metabolism.energy, metabolism.max_energy);
        EXPECT_GT(lifecycle.size_scale, 0.0);
    }
    EXPECT_GT(count, 0U);
}

TEST_F(RenderClientFixture, ECSPlantComponents) {
    setup_simulation_with_entities(0, 10);

    auto& reg = registry();
    auto view = reg.view<const evolution::sim::TransformComponent,
                         const evolution::sim::PlantComponent>();

    std::size_t count = 0;
    std::size_t alive_count = 0;
    for (auto entity : view) {
        ++count;
        const auto& transform = view.get<const evolution::sim::TransformComponent>(entity);
        const auto& plant = view.get<const evolution::sim::PlantComponent>(entity);

        EXPECT_FALSE(std::isnan(transform.position.x));
        EXPECT_GT(plant.max_energy, 0.0);
        EXPECT_GE(plant.energy, 0.0);
        EXPECT_GT(plant.radius, 0.0);
        EXPECT_LT(plant.species_id, 6U);  // Should be < palette size

        if (plant.alive) {
            ++alive_count;
        }
    }

    EXPECT_GT(count, 0U);
    EXPECT_GT(alive_count, 0U);
}

TEST_F(RenderClientFixture, ECSTransformPositions) {
    setup_simulation_with_entities(3, 5);

    auto& reg = registry();
    auto transform_view = reg.view<const evolution::sim::TransformComponent>();

    std::size_t count = 0;
    for (auto entity : transform_view) {
        ++count;
        const auto& transform = transform_view.get<const evolution::sim::TransformComponent>(entity);
        const glm::vec3 pos = to_glm(transform.position);

        // Verify positions are valid
        EXPECT_FALSE(std::isnan(pos.x));
        EXPECT_FALSE(std::isnan(pos.y));
        EXPECT_FALSE(std::isnan(pos.z));
        EXPECT_FALSE(std::isinf(pos.x));
        EXPECT_FALSE(std::isinf(pos.y));
        EXPECT_FALSE(std::isinf(pos.z));
    }
    EXPECT_GT(count, 0U);
}

TEST_F(RenderClientFixture, ECSColliderScales) {
    setup_simulation_with_entities(5, 0);

    auto& reg = registry();
    auto view = reg.view<const evolution::sim::ColliderComponent,
                         const evolution::sim::LifecycleComponent>();

    for (auto entity : view) {
        const auto& collider = view.get<const evolution::sim::ColliderComponent>(entity);
        const auto& lifecycle = view.get<const evolution::sim::LifecycleComponent>(entity);

        // Verify collider has valid dimensions
        switch (collider.type) {
            case evolution::sim::ShapeType::Sphere:
                EXPECT_GT(collider.sphere.radius, 0.0);
                break;
            case evolution::sim::ShapeType::CapsuleY:
                EXPECT_GT(collider.capsule.radius, 0.0);
                EXPECT_GT(collider.capsule.half_height, 0.0);
                break;
            case evolution::sim::ShapeType::Aabb:
                EXPECT_GT(collider.aabb.half_extents.x, 0.0);
                EXPECT_GT(collider.aabb.half_extents.y, 0.0);
                EXPECT_GT(collider.aabb.half_extents.z, 0.0);
                break;
            default:
                FAIL() << "Unknown collider type";
        }

        EXPECT_GT(lifecycle.size_scale, 0.0);
    }
}

TEST_F(RenderClientFixture, ECSEnergyRatios) {
    setup_simulation_with_entities(5, 10);

    auto& reg = registry();

    // Test agent energy ratios
    auto agent_view = reg.view<const evolution::sim::MetabolismComponent>();
    for (auto entity : agent_view) {
        const auto& metabolism = agent_view.get<const evolution::sim::MetabolismComponent>(entity);
        const double ratio = metabolism.energy / std::max(1.0, metabolism.max_energy);
        EXPECT_GE(ratio, 0.0);
        EXPECT_LE(ratio, 1.0);
    }

    // Test plant energy ratios
    auto plant_view = reg.view<const evolution::sim::PlantComponent>();
    for (auto entity : plant_view) {
        const auto& plant = plant_view.get<const evolution::sim::PlantComponent>(entity);
        if (plant.alive) {
            const double ratio = plant.energy / std::max(1.0, plant.max_energy);
            EXPECT_GE(ratio, 0.0);
            EXPECT_LE(ratio, 1.0);
        }
    }
}

}  // namespace evolution::client::test

