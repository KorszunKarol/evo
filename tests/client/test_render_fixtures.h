#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/simulation_app.h"

namespace evolution::client::test {

/**
 * @brief Manages a headless OpenGL context for testing.
 *
 * Creates an offscreen OpenGL context that can be used for testing rendering code
 * without requiring a display server. Uses GLFW with a hidden window.
 */
class OpenGLTestContext {
public:
    OpenGLTestContext();
    ~OpenGLTestContext();

    // Non-copyable
    OpenGLTestContext(const OpenGLTestContext&) = delete;
    OpenGLTestContext& operator=(const OpenGLTestContext&) = delete;

    // Movable
    OpenGLTestContext(OpenGLTestContext&&) noexcept = default;
    OpenGLTestContext& operator=(OpenGLTestContext&&) noexcept = default;

    /**
     * @brief Check if context was successfully created.
     */
    [[nodiscard]] bool is_valid() const { return window_ != nullptr; }

    /**
     * @brief Get the GLFW window handle.
     */
    [[nodiscard]] GLFWwindow* window() const { return window_; }

    /**
     * @brief Make this context current.
     */
    void make_current();

    /**
     * @brief Clear any OpenGL errors.
     */
    void clear_errors();

    /**
     * @brief Check for OpenGL errors and fail test if any found.
     */
    void check_gl_errors(const char* operation);

private:
    GLFWwindow* window_{nullptr};
    bool glfw_initialized_{false};
};

/**
 * @brief Test fixture providing OpenGL context and simulation setup.
 */
class RenderClientFixture : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    /**
     * @brief Create a minimal simulation with terrain and entities.
     */
    void setup_minimal_simulation();

    /**
     * @brief Create a simulation with agents and plants.
     */
    void setup_simulation_with_entities(std::size_t agent_count = 10,
                                        std::size_t plant_count = 20);

    /**
     * @brief Access OpenGL context.
     */
    [[nodiscard]] OpenGLTestContext& gl_context() { return *gl_context_; }

    /**
     * @brief Access simulation app.
     */
    [[nodiscard]] evolution::sim::SimulationApp& app() { return app_; }

    /**
     * @brief Access registry.
     */
    [[nodiscard]] entt::registry& registry() { return app_.registry(); }

private:
    std::unique_ptr<OpenGLTestContext> gl_context_;
    evolution::sim::SimulationApp app_;
    evolution::genetics::GenomeStorage genome_storage_;
};

/**
 * @brief Helper to create a test terrain.
 */
[[nodiscard]] std::unique_ptr<evolution::sim::Terrain> create_test_terrain(
    std::uint32_t seed = 42);

/**
 * @brief Helper to create a test biome map.
 */
[[nodiscard]] std::unique_ptr<evolution::sim::BiomeMap> create_test_biome_map(
    const evolution::sim::Terrain& terrain, std::uint32_t seed = 42);

/**
 * @brief Helper to create a test water map.
 */
[[nodiscard]] std::unique_ptr<evolution::sim::WaterMap> create_test_water_map(
    const evolution::sim::Terrain& terrain, std::uint32_t seed = 42);

}  // namespace evolution::client::test

