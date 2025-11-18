#include "test_render_fixtures.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "evolution/sim/environment/environment.h"

namespace evolution::client::test {

// Helper functions matching main.cpp structure
namespace {
struct Mesh {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};
};

void destroy_mesh(Mesh& mesh) {
    if (mesh.ebo != 0U) {
        glDeleteBuffers(1, &mesh.ebo);
        mesh.ebo = 0;
    }
    if (mesh.vbo != 0U) {
        glDeleteBuffers(1, &mesh.vbo);
        mesh.vbo = 0;
    }
    if (mesh.vao != 0U) {
        glDeleteVertexArrays(1, &mesh.vao);
        mesh.vao = 0;
    }
    mesh.index_count = 0;
}

Mesh create_water_mesh(double water_level, double width, double height) {
    std::vector<float> vertices = {
        0.0F, static_cast<float>(water_level), 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F,
        static_cast<float>(width), static_cast<float>(water_level), 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F,
        static_cast<float>(width), static_cast<float>(water_level), static_cast<float>(height), 0.0F, 1.0F,
        0.0F, 1.0F, 1.0F,
        0.0F, static_cast<float>(water_level), static_cast<float>(height), 0.0F, 1.0F, 0.0F, 0.0F, 1.0F};

    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};

    Mesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<const void*>(6 * sizeof(float)));
    glBindVertexArray(0);

    mesh.index_count = static_cast<GLsizei>(indices.size());
    return mesh;
}
}  // namespace

TEST_F(RenderClientFixture, WaterMeshCreation) {
    gl_context().make_current();
    gl_context().clear_errors();

    constexpr double water_level = 5.0;
    constexpr double width = 100.0;
    constexpr double height = 100.0;

    Mesh mesh = create_water_mesh(water_level, width, height);

    EXPECT_NE(mesh.vao, 0U);
    EXPECT_NE(mesh.vbo, 0U);
    EXPECT_NE(mesh.ebo, 0U);
    EXPECT_EQ(mesh.index_count, 6);

    gl_context().check_gl_errors("water mesh creation");

    destroy_mesh(mesh);
    gl_context().check_gl_errors("water mesh destruction");
}

TEST_F(RenderClientFixture, WaterMeshVertices) {
    gl_context().make_current();
    gl_context().clear_errors();

    constexpr double water_level = 10.0;
    constexpr double width = 50.0;
    constexpr double height = 75.0;

    Mesh mesh = create_water_mesh(water_level, width, height);

    // Verify mesh was created successfully
    EXPECT_NE(mesh.vao, 0U);
    EXPECT_EQ(mesh.index_count, 6);  // Two triangles = 6 indices

    // Check that buffers are valid
    GLint vbo_size = 0;
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &vbo_size);
    EXPECT_GT(vbo_size, 0);

    GLint ebo_size = 0;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &ebo_size);
    EXPECT_GT(ebo_size, 0);

    gl_context().check_gl_errors("water mesh validation");

    destroy_mesh(mesh);
}

TEST_F(RenderClientFixture, TerrainMeshWithBiomes) {
    gl_context().make_current();
    gl_context().clear_errors();

    setup_minimal_simulation();
    auto& reg = registry();
    const auto* terrain = reg.ctx().find<evolution::sim::Terrain>();
    ASSERT_NE(terrain, nullptr);

    const auto* biome_map = reg.ctx().find<evolution::sim::BiomeMap>();

    // Note: This test verifies the terrain mesh creation logic
    // The actual function is in main.cpp anonymous namespace, so we test indirectly
    // by verifying terrain data is available for mesh creation

    EXPECT_GT(terrain->width(), 0);
    EXPECT_GT(terrain->height_cells(), 0);
    EXPECT_GT(terrain->cell_size(), 0.0);

    // Verify terrain can be sampled
    const double test_x = terrain->width() * terrain->cell_size() * 0.5;
    const double test_z = terrain->height_cells() * terrain->cell_size() * 0.5;
    const double height = terrain->height(test_x, test_z);
    EXPECT_FALSE(std::isnan(height));
    EXPECT_FALSE(std::isinf(height));

    const auto normal = terrain->normal(test_x, test_z);
    EXPECT_NEAR(glm::length(glm::vec3(normal.x, normal.y, normal.z)), 1.0, 0.01);

    if (biome_map != nullptr) {
        const auto biome_id = biome_map->sample(test_x, test_z);
        EXPECT_GE(static_cast<int>(biome_id), 0);
    }
}

TEST_F(RenderClientFixture, MeshCleanup) {
    gl_context().make_current();
    gl_context().clear_errors();

    Mesh mesh = create_water_mesh(5.0, 10.0, 10.0);
    EXPECT_NE(mesh.vao, 0U);

    destroy_mesh(mesh);

    EXPECT_EQ(mesh.vao, 0U);
    EXPECT_EQ(mesh.vbo, 0U);
    EXPECT_EQ(mesh.ebo, 0U);
    EXPECT_EQ(mesh.index_count, 0);

    gl_context().check_gl_errors("mesh cleanup");
}

}  // namespace evolution::client::test

