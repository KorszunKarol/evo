#include "test_render_fixtures.h"

#include <cstdio>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/scenario.h"

namespace evolution::client::test {

OpenGLTestContext::OpenGLTestContext() {
    glfwSetErrorCallback([](int code, const char* description) {
        std::fprintf(stderr, "GLFW error (%d): %s\n", code, description);
    });

    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfw_initialized_ = true;

    // Request OpenGL 3.3 core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);  // Hidden window for headless testing

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window_ = glfwCreateWindow(640, 480, "Test Window", nullptr, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // Set up basic OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

OpenGLTestContext::~OpenGLTestContext() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
    if (glfw_initialized_) {
        glfwTerminate();
    }
}

void OpenGLTestContext::make_current() {
    if (window_ != nullptr) {
        glfwMakeContextCurrent(window_);
    }
}

void OpenGLTestContext::clear_errors() {
    while (glGetError() != GL_NO_ERROR) {
        // Clear all errors
    }
}

void OpenGLTestContext::check_gl_errors(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        const char* error_str = "Unknown";
        switch (error) {
            case GL_INVALID_ENUM:
                error_str = "GL_INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                error_str = "GL_INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                error_str = "GL_INVALID_OPERATION";
                break;
            case GL_OUT_OF_MEMORY:
                error_str = "GL_OUT_OF_MEMORY";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                error_str = "GL_INVALID_FRAMEBUFFER_OPERATION";
                break;
        }
        FAIL() << "OpenGL error in " << operation << ": " << error_str << " (0x"
               << std::hex << error << ")";
    }
}

void RenderClientFixture::SetUp() {
    ::testing::Test::SetUp();

    spdlog::set_level(spdlog::level::warn);  // Reduce noise in tests

    try {
        gl_context_ = std::make_unique<OpenGLTestContext>();
        if (!gl_context_->is_valid()) {
            GTEST_SKIP() << "Failed to create OpenGL context (may not be available in CI)";
        }
        gl_context_->make_current();
        gl_context_->clear_errors();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "OpenGL context creation failed: " << e.what()
                     << " (may not be available in CI)";
    }
}

void RenderClientFixture::TearDown() {
    gl_context_.reset();
    ::testing::Test::TearDown();
}

void RenderClientFixture::setup_minimal_simulation() {
    evolution::sim::SimulationScenario scenario{};
    scenario.environment.terrain.width_cells = 64;
    scenario.environment.terrain.height_cells = 64;
    scenario.environment.terrain.cell_size = 2.0;
    scenario.environment.terrain.elevation_scale = 18.0;
    scenario.environment.terrain.seed = 42;

    scenario.environment.soil.width_cells = 32;
    scenario.environment.soil.height_cells = 32;
    scenario.environment.soil.cell_size = 4.0;

    scenario.environment.biome.biome_count = 4;
    scenario.environment.water.water_level_percentile = 0.22;
    scenario.environment.plants.initial_count = 0;
    scenario.initial_population = 0;

    evolution::sim::setup_scenario(app_, genome_storage_, scenario);
}

void RenderClientFixture::setup_simulation_with_entities(std::size_t agent_count,
                                                           std::size_t plant_count) {
    setup_minimal_simulation();

    auto& registry = app_.registry();
    const auto* terrain = registry.ctx().find<evolution::sim::Terrain>();
    if (terrain == nullptr) {
        return;
    }

    // Spawn plants
    for (std::size_t i = 0; i < plant_count; ++i) {
        const double x = static_cast<double>(i % 8) * 4.0;
        const double z = static_cast<double>(i / 8) * 4.0;
        const double y = terrain->height(x, z);

        const entt::entity plant = registry.create();
        registry.emplace<evolution::sim::TransformComponent>(
            plant, evolution::sim::TransformComponent{.position = {x, y, z}});

        evolution::sim::PlantComponent plant_comp{};
        plant_comp.species_id = static_cast<std::uint8_t>(i % 5);
        plant_comp.energy = 10.0 + static_cast<double>(i % 10);
        plant_comp.max_energy = 20.0;
        plant_comp.growth_rate = 2.0;
        plant_comp.radius = 0.6;
        plant_comp.alive = true;
        registry.emplace<evolution::sim::PlantComponent>(plant, plant_comp);
    }

    // Spawn agents
    for (std::size_t i = 0; i < agent_count; ++i) {
        const double x = static_cast<double>(i % 8) * 4.0 + 2.0;
        const double z = static_cast<double>(i / 8) * 4.0 + 2.0;
        const double y = terrain->height(x, z) + 1.0;

        const entt::entity agent = registry.create();
        registry.emplace<evolution::sim::TransformComponent>(
            agent, evolution::sim::TransformComponent{.position = {x, y, z}});

        evolution::sim::KinematicsComponent kinematics{};
        kinematics.inverse_mass = 1.0;
        kinematics.linear_damping = 0.2;
        registry.emplace<evolution::sim::KinematicsComponent>(agent, kinematics);

        evolution::sim::ColliderComponent collider{};
        collider.type = evolution::sim::ShapeType::Sphere;
        collider.sphere.radius = 0.5;
        registry.emplace<evolution::sim::ColliderComponent>(agent, collider);

        evolution::sim::MetabolismComponent metabolism{};
        metabolism.energy = 50.0 + static_cast<double>(i % 50);
        metabolism.max_energy = 100.0;
        metabolism.basal_rate = 1.0;
        registry.emplace<evolution::sim::MetabolismComponent>(agent, metabolism);

        evolution::sim::LifecycleComponent lifecycle{};
        lifecycle.size_scale = 1.0;
        registry.emplace<evolution::sim::LifecycleComponent>(agent, lifecycle);

        registry.emplace<evolution::sim::HerbivoreTag>(agent);
    }
}

std::unique_ptr<evolution::sim::Terrain> create_test_terrain(std::uint32_t seed) {
    evolution::sim::TerrainConfig config{};
    config.width_cells = 64;
    config.height_cells = 64;
    config.cell_size = 2.0;
    config.elevation_scale = 18.0;
    config.octaves = 5;
    config.base_frequency = 0.004;
    config.seed = seed;

    return std::make_unique<evolution::sim::Terrain>(config);
}

std::unique_ptr<evolution::sim::BiomeMap> create_test_biome_map(
    const evolution::sim::Terrain& terrain, std::uint32_t seed) {
    evolution::sim::BiomeConfig config{};
    config.width_cells = terrain.width();
    config.height_cells = terrain.height_cells();
    config.cell_size = terrain.cell_size();
    config.seed = seed;
    config.biome_count = 4;
    config.octaves = 4;
    config.base_frequency = 0.003;

    return std::make_unique<evolution::sim::BiomeMap>(config, terrain);
}

std::unique_ptr<evolution::sim::WaterMap> create_test_water_map(
    const evolution::sim::Terrain& terrain, std::uint32_t seed) {
    evolution::sim::WaterConfig config{};
    config.width_cells = terrain.width();
    config.height_cells = terrain.height_cells();
    config.cell_size = terrain.cell_size();
    config.seed = seed;
    config.water_level_percentile = 0.25;
    config.min_flow_accumulation = 500.0;

    return std::make_unique<evolution::sim::WaterMap>(config, terrain);
}

}  // namespace evolution::client::test

