#include <array>
#include <cstdio>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/physics/simple_backend.h"
#include "evolution/sim/physics_system.h"
#include "evolution/sim/simulation_app.h"

namespace evo = evolution::sim;

/**
 * @brief Converts evolution::sim::Vec3 to glm::vec3.
 */
[[nodiscard]] glm::vec3 to_glm(const evo::Vec3& v) {
    return glm::vec3(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
}

/**
 * @brief Simple orbit camera state.
 */
struct OrbitCamera {
    glm::vec3 target{0.0F, 10.0F, 0.0F};
    float distance{50.0F};
    float yaw{0.0F};
    float pitch{0.5F};
};

/**
 * @brief Computes camera position from orbit parameters.
 */
[[nodiscard]] glm::vec3 compute_camera_position(const OrbitCamera& camera) {
    const float cp = std::cos(camera.pitch);
    const float sp = std::sin(camera.pitch);
    const float cy = std::cos(camera.yaw);
    const float sy = std::sin(camera.yaw);
    return camera.target + glm::vec3(camera.distance * cp * cy, camera.distance * sp,
                                     camera.distance * cp * sy);
}

/**
 * @brief Updates camera from keyboard input.
 */
void update_camera(GLFWwindow* window, OrbitCamera& camera, float delta_seconds) {
    const float rotation_speed = glm::radians(60.0F);
    const float zoom_speed = 8.0F;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        camera.yaw -= rotation_speed * delta_seconds;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        camera.yaw += rotation_speed * delta_seconds;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        camera.pitch += rotation_speed * delta_seconds;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        camera.pitch -= rotation_speed * delta_seconds;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.distance -= zoom_speed * delta_seconds;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.distance += zoom_speed * delta_seconds;
    }
    camera.pitch = std::clamp(camera.pitch, glm::radians(-80.0F), glm::radians(80.0F));
    camera.distance = std::clamp(camera.distance, 5.0F, 200.0F);
}

/**
 * @brief Compiles a shader from source.
 */
[[nodiscard]] GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == 0) {
        GLchar info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        throw std::runtime_error(std::string("Shader compilation failed: ") + info_log);
    }
    return shader;
}

/**
 * @brief Links shaders into a program.
 */
[[nodiscard]] GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == 0) {
        GLchar info_log[512];
        glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
        throw std::runtime_error(std::string("Program linking failed: ") + info_log);
    }
    return program;
}

/**
 * @brief Generates a mesh from terrain heightfield data.
 */
struct TerrainMesh {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};
};

/**
 * @brief Creates a terrain mesh from the terrain heightfield.
 */
[[nodiscard]] TerrainMesh create_terrain_mesh(const evo::Terrain& terrain) {
    const int width = terrain.width();
    const int height = terrain.height_cells();
    const double cell_size = terrain.cell_size();

    // Generate vertices with color attribute (for consistent vertex layout)
    std::vector<float> vertices;
    vertices.reserve(static_cast<std::size_t>(width * height * 9));  // pos(3) + normal(3) + color(3)

    for (int iz = 0; iz < height; ++iz) {
        for (int ix = 0; ix < width; ++ix) {
            const double x = static_cast<double>(ix) * cell_size;
            const double z = static_cast<double>(iz) * cell_size;
            const double y = terrain.height(x, z);
            const evo::Vec3 n = terrain.normal(x, z);

            vertices.push_back(static_cast<float>(x));
            vertices.push_back(static_cast<float>(y));
            vertices.push_back(static_cast<float>(z));
            vertices.push_back(static_cast<float>(n.x));
            vertices.push_back(static_cast<float>(n.y));
            vertices.push_back(static_cast<float>(n.z));
            vertices.push_back(0.5F);  // Default color (unused when uUseVertexColor is false)
            vertices.push_back(0.8F);
            vertices.push_back(0.4F);
        }
    }

    // Generate indices for triangles
    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>((width - 1) * (height - 1) * 6));

    for (int iz = 0; iz < height - 1; ++iz) {
        for (int ix = 0; ix < width - 1; ++ix) {
            const std::uint32_t i0 = static_cast<std::uint32_t>(iz * width + ix);
            const std::uint32_t i1 = static_cast<std::uint32_t>(iz * width + ix + 1);
            const std::uint32_t i2 = static_cast<std::uint32_t>((iz + 1) * width + ix);
            const std::uint32_t i3 = static_cast<std::uint32_t>((iz + 1) * width + ix + 1);

            // First triangle
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            // Second triangle
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    TerrainMesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
                          reinterpret_cast<const void*>(6 * sizeof(float)));

    glBindVertexArray(0);
    mesh.index_count = static_cast<GLsizei>(indices.size());
    return mesh;
}

/**
 * @brief Destroys terrain mesh resources.
 */
void destroy_terrain_mesh(TerrainMesh& mesh) {
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

/**
 * @brief Creates a water plane mesh at the specified water level.
 *
 * @param water_level double Water level height in meters.
 * @param width double World-space width of water plane.
 * @param height double World-space height of water plane.
 * @return TerrainMesh Water mesh (reusing TerrainMesh structure).
 */
[[nodiscard]] TerrainMesh create_water_mesh(double water_level, double width, double height) {
    std::vector<float> vertices = {
        // Position (3) + Normal (3) + UV (2)
        0.0F, static_cast<float>(water_level), 0.0F,          0.0F, 1.0F, 0.0F,  0.0F, 0.0F,
        static_cast<float>(width), static_cast<float>(water_level), 0.0F,          0.0F, 1.0F, 0.0F,  1.0F, 0.0F,
        static_cast<float>(width), static_cast<float>(water_level), static_cast<float>(height), 0.0F, 1.0F, 0.0F,  1.0F, 1.0F,
        0.0F, static_cast<float>(water_level), static_cast<float>(height), 0.0F, 1.0F, 0.0F,  0.0F, 1.0F
    };

    std::vector<std::uint32_t> indices = {
        0, 1, 2,
        0, 2, 3
    };

    TerrainMesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
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

/**
 * @brief Creates a terrain mesh with biome color data.
 *
 * @param terrain const evo::Terrain& Terrain reference.
 * @param biome_map const evo::BiomeMap* Optional biome map (nullptr if not available).
 * @return TerrainMesh Terrain mesh with biome colors.
 */
[[nodiscard]] TerrainMesh create_terrain_mesh_with_biomes(const evo::Terrain& terrain,
                                                           const evo::BiomeMap* biome_map) {
    const int width = terrain.width();
    const int height = terrain.height_cells();
    const double cell_size = terrain.cell_size();

    // Biome color LUT: [Plains, Forest, Wetland, Alpine]
    constexpr glm::vec3 biome_colors[4] = {
        glm::vec3(0.6F, 0.8F, 0.4F),  // Plains: light green
        glm::vec3(0.2F, 0.5F, 0.2F),  // Forest: dark green
        glm::vec3(0.4F, 0.6F, 0.5F),  // Wetland: teal
        glm::vec3(0.7F, 0.7F, 0.8F)   // Alpine: light gray
    };

    // Generate vertices with biome colors
    std::vector<float> vertices;
    vertices.reserve(static_cast<std::size_t>(width * height * 9));  // pos(3) + normal(3) + color(3)

    for (int iz = 0; iz < height; ++iz) {
        for (int ix = 0; ix < width; ++ix) {
            const double x = static_cast<double>(ix) * cell_size;
            const double z = static_cast<double>(iz) * cell_size;
            const double y = terrain.height(x, z);
            const evo::Vec3 n = terrain.normal(x, z);

            glm::vec3 color = glm::vec3(0.5F, 0.8F, 0.4F);  // Default terrain color
            if (biome_map != nullptr) {
                const evo::BiomeId biome = biome_map->sample(x, z);
                const int biome_idx = static_cast<int>(biome);
                if (biome_idx >= 0 && biome_idx < 4) {
                    color = biome_colors[biome_idx];
                }
            }

            vertices.push_back(static_cast<float>(x));
            vertices.push_back(static_cast<float>(y));
            vertices.push_back(static_cast<float>(z));
            vertices.push_back(static_cast<float>(n.x));
            vertices.push_back(static_cast<float>(n.y));
            vertices.push_back(static_cast<float>(n.z));
            vertices.push_back(color.r);
            vertices.push_back(color.g);
            vertices.push_back(color.b);
        }
    }

    // Generate indices (same as regular terrain)
    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>((width - 1) * (height - 1) * 6));

    for (int iz = 0; iz < height - 1; ++iz) {
        for (int ix = 0; ix < width - 1; ++ix) {
            const std::uint32_t i0 = static_cast<std::uint32_t>(iz * width + ix);
            const std::uint32_t i1 = static_cast<std::uint32_t>(iz * width + ix + 1);
            const std::uint32_t i2 = static_cast<std::uint32_t>((iz + 1) * width + ix);
            const std::uint32_t i3 = static_cast<std::uint32_t>((iz + 1) * width + ix + 1);

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    TerrainMesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
                          reinterpret_cast<const void*>(6 * sizeof(float)));

    glBindVertexArray(0);
    mesh.index_count = static_cast<GLsizei>(indices.size());
    return mesh;
}

/**
 * @brief GLFW error callback.
 */
void glfw_error_callback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error (%d): %s\n", code, description);
}

/**
 * @brief Entry point for terrain visualization client.
 */
int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() != GLFW_TRUE) {
        std::fprintf(stderr, "Failed to initialize GLFW.\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Evolution Terrain Viewer", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "Failed to create GLFW window.\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "Failed to initialize GLAD.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    const char* vertex_shader_src = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out vec3 vWorldPos;
out vec3 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(uModel)));
    vNormal = normalize(normalMatrix * aNormal);
    vColor = aColor;
    gl_Position = uProjection * uView * worldPos;
}
)";

    const char* fragment_shader_src = R"(#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec3 vColor;

uniform vec3 uColor;
uniform vec3 uViewPos;
uniform bool uUseVertexColor;

out vec4 FragColor;

void main() {
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));
    vec3 norm = normalize(vNormal);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 base_color = uUseVertexColor ? vColor : uColor;
    vec3 base = base_color * (0.2 + 0.8 * diff);
    FragColor = vec4(base, 1.0);
}
)";

    const char* water_vertex_shader_src = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;
out vec2 vUV;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vUV = aUV;
    gl_Position = uProjection * uView * worldPos;
}
)";

    const char* water_fragment_shader_src = R"(#version 330 core
in vec3 vWorldPos;
in vec2 vUV;

uniform vec3 uViewPos;
uniform float uTime;
uniform float uWaterLevel;

out vec4 FragColor;

void main() {
    // Simple water shader with depth-based color and Fresnel-like effect
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    vec3 normal = vec3(0.0, 1.0, 0.0);
    
    // Cheap Fresnel approximation
    float fresnel = pow(1.0 - max(dot(viewDir, normal), 0.0), 2.0);
    
    // Depth-based color (shallow = light blue, deep = dark blue)
    float depth_factor = clamp((vWorldPos.y - uWaterLevel + 2.0) / 4.0, 0.0, 1.0);
    vec3 shallow_color = vec3(0.2, 0.6, 0.9);
    vec3 deep_color = vec3(0.05, 0.2, 0.4);
    vec3 water_color = mix(shallow_color, deep_color, depth_factor);
    
    // Add slight animation
    float wave = sin(vWorldPos.x * 0.1 + uTime) * 0.02 + sin(vWorldPos.z * 0.15 + uTime * 1.2) * 0.02;
    water_color += vec3(wave * 0.1);
    
    // Combine with Fresnel
    vec3 final_color = mix(water_color, vec3(0.8, 0.9, 1.0), fresnel * 0.3);
    
    FragColor = vec4(final_color, 0.7);  // Semi-transparent
}
)";

    GLuint program = 0;
    try {
        program = link_program(compile_shader(GL_VERTEX_SHADER, vertex_shader_src),
                                compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src));
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "Shader error: %s\n", ex.what());
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const GLint uniform_model = glGetUniformLocation(program, "uModel");
    const GLint uniform_view = glGetUniformLocation(program, "uView");
    const GLint uniform_projection = glGetUniformLocation(program, "uProjection");
    const GLint uniform_color = glGetUniformLocation(program, "uColor");
    const GLint uniform_view_pos = glGetUniformLocation(program, "uViewPos");
    const GLint uniform_use_vertex_color = glGetUniformLocation(program, "uUseVertexColor");

    // Create water shader program
    GLuint water_program = 0;
    try {
        water_program = link_program(compile_shader(GL_VERTEX_SHADER, water_vertex_shader_src),
                                      compile_shader(GL_FRAGMENT_SHADER, water_fragment_shader_src));
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "Water shader error: %s\n", ex.what());
        glDeleteProgram(program);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const GLint water_uniform_model = glGetUniformLocation(water_program, "uModel");
    const GLint water_uniform_view = glGetUniformLocation(water_program, "uView");
    const GLint water_uniform_projection = glGetUniformLocation(water_program, "uProjection");
    const GLint water_uniform_view_pos = glGetUniformLocation(water_program, "uViewPos");
    const GLint water_uniform_time = glGetUniformLocation(water_program, "uTime");
    const GLint water_uniform_water_level = glGetUniformLocation(water_program, "uWaterLevel");

    // Initialize simulation with terrain (no physics, no entities)
    evo::SimulationApp app;
    evo::TerrainConfig terrain_config{};
    terrain_config.width_cells = 256;
    terrain_config.height_cells = 256;
    terrain_config.cell_size = 2.0;
    terrain_config.elevation_scale = 30.0;
    terrain_config.seed = 1337;

    evo::SoilConfig soil_config{};
    soil_config.width_cells = 128;
    soil_config.height_cells = 128;
    soil_config.cell_size = 4.0;

    evo::EnvironmentConfig env_config{};
    env_config.terrain = terrain_config;
    env_config.soil = soil_config;
    env_config.biome.biome_count = 3;
    env_config.water.water_level_percentile = 0.25;

    // Initialize environment directly
    evo::initialize_environment(app.registry(), env_config);
    evo::seed_initial_plants(app.registry(), env_config);
    evo::update_environment_stats(app.registry());

    // Get terrain, biome map, and water map from context
    auto* terrain_ptr = app.registry().ctx().find<evo::Terrain>();
    auto* biome_map_ptr = app.registry().ctx().find<evo::BiomeMap>();
    auto* water_map_ptr = app.registry().ctx().find<evo::WaterMap>();
    auto* env_stats_ptr = app.registry().ctx().find<evo::EnvironmentStats>();
    
    if (terrain_ptr == nullptr) {
        std::fprintf(stderr, "Failed to find terrain in registry context.\n");
        glDeleteProgram(program);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const evo::Terrain& terrain = *terrain_ptr;
    const evo::BiomeMap* biome_map = biome_map_ptr;
    const evo::WaterMap* water_map = water_map_ptr;
    evo::EnvironmentStats* environment_stats = env_stats_ptr;
    
    // Debug: Print terrain info
    std::printf("Terrain loaded: %d x %d cells, cell_size=%.2f\n", 
                terrain.width(), terrain.height_cells(), terrain.cell_size());
    std::printf("Height range: %.2f to %.2f\n", terrain.min_y(), terrain.max_y());
    
    // Create terrain mesh (with biome colors if available)
    TerrainMesh terrain_mesh = create_terrain_mesh_with_biomes(terrain, biome_map);
    std::printf("Terrain mesh created: %zu indices\n", static_cast<std::size_t>(terrain_mesh.index_count));

    // Create water mesh if water map is available
    TerrainMesh water_mesh{};
    double water_level = 0.0;
    if (water_map != nullptr) {
        water_level = water_map->water_level();
        const float terrain_width = static_cast<float>(terrain.width() * terrain.cell_size());
        const float terrain_height = static_cast<float>(terrain.height_cells() * terrain.cell_size());
        water_mesh = create_water_mesh(water_level, terrain_width, terrain_height);
        std::printf("Water mesh created at level %.2f\n", water_level);
    }

    // Calculate terrain center and average height for camera
    const float terrain_width = static_cast<float>(terrain.width() * terrain.cell_size());
    const float terrain_height = static_cast<float>(terrain.height_cells() * terrain.cell_size());
    const float center_x = terrain_width * 0.5F;
    const float center_z = terrain_height * 0.5F;
    const float avg_height = static_cast<float>((terrain.min_y() + terrain.max_y()) * 0.5);
    
    std::printf("Terrain world size: %.1f x %.1f m\n", terrain_width, terrain_height);
    std::printf("Camera target: (%.1f, %.1f, %.1f)\n", center_x, avg_height, center_z);

    OrbitCamera camera{};
    camera.target = glm::vec3(center_x, avg_height, center_z);
    camera.distance = terrain_width * 1.2F;  // Distance based on terrain size
    camera.pitch = 0.6F;  // Look down at terrain
    camera.yaw = 0.0F;

    const double fixed_dt = app.fixed_dt();
    double accumulator = 0.0;
    double last_time = glfwGetTime();
    bool show_biome_overlay = false;
    bool biome_overlay_key_pressed = false;

    while (!glfwWindowShouldClose(window)) {
        const double current_time = glfwGetTime();
        const double delta = current_time - last_time;
        last_time = current_time;
        accumulator += delta;

        glfwPollEvents();
        evo::update_environment_stats(app.registry());
        environment_stats = app.registry().ctx().find<evo::EnvironmentStats>();
        update_camera(window, camera, static_cast<float>(delta));

        // Toggle biome overlay with 'B' key
        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
            if (!biome_overlay_key_pressed) {
                show_biome_overlay = !show_biome_overlay;
                biome_overlay_key_pressed = true;
            }
        } else {
            biome_overlay_key_pressed = false;
        }

        // Don't run simulation - just show terrain
        // (Uncomment below if you want to see simulation running)
        // while (accumulator >= fixed_dt) {
        //     app.tick();
        //     accumulator -= fixed_dt;
        // }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.1F, 0.1F, 0.15F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const glm::vec3 eye = compute_camera_position(camera);
        const glm::mat4 view = glm::lookAt(eye, camera.target, glm::vec3(0.0F, 1.0F, 0.0F));
        const float aspect = (height != 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0F;
        // Increase far plane to ensure terrain is visible
        const glm::mat4 projection = glm::perspective(glm::radians(45.0F), aspect, 1.0F, 2000.0F);

        // Draw terrain
        glUseProgram(program);
        glUniformMatrix4fv(uniform_view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uniform_projection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(uniform_view_pos, 1, glm::value_ptr(eye));

        const glm::mat4 model = glm::mat4(1.0F);
        glUniformMatrix4fv(uniform_model, 1, GL_FALSE, glm::value_ptr(model));
        
        if (show_biome_overlay && biome_map != nullptr) {
            // Use vertex colors from biome mesh
            glUniform1i(uniform_use_vertex_color, 1);
            glUniform3fv(uniform_color, 1, glm::value_ptr(glm::vec3(1.0F, 1.0F, 1.0F)));  // White (colors come from vertices)
        } else {
            // Use uniform color
            glUniform1i(uniform_use_vertex_color, 0);
            glUniform3fv(uniform_color, 1, glm::value_ptr(glm::vec3(0.5F, 0.8F, 0.4F)));  // Bright green terrain
        }
        
        glBindVertexArray(terrain_mesh.vao);
        glDrawElements(GL_TRIANGLES, terrain_mesh.index_count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        // Draw water (if available)
        if (water_map != nullptr && water_mesh.vao != 0U) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);  // Allow terrain to show through

            glUseProgram(water_program);
            glUniformMatrix4fv(water_uniform_view, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(water_uniform_projection, 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3fv(water_uniform_view_pos, 1, glm::value_ptr(eye));
            glUniformMatrix4fv(water_uniform_model, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1f(water_uniform_time, static_cast<float>(glfwGetTime()));
            glUniform1f(water_uniform_water_level, static_cast<float>(water_level));

            glBindVertexArray(water_mesh.vao);
            glDrawElements(GL_TRIANGLES, water_mesh.index_count, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // ImGui overlay
        ImGui::Begin("Terrain Info");
        ImGui::Text("Terrain Size: %d x %d cells", terrain.width(), terrain.height_cells());
        ImGui::Text("World Size: %.1f x %.1f m", terrain_width, terrain_height);
        ImGui::Text("Cell Size: %.2f m", terrain.cell_size());
        ImGui::Text("Height Range: %.2f - %.2f m", terrain.min_y(), terrain.max_y());
        ImGui::Text("Center: (%.1f, %.1f, %.1f)", center_x, avg_height, center_z);
        ImGui::Separator();
        ImGui::Text("Camera Distance: %.2f", camera.distance);
        ImGui::Text("Camera Target: (%.1f, %.1f, %.1f)", camera.target.x, camera.target.y, camera.target.z);
        ImGui::Text("Yaw: %.1f deg", glm::degrees(camera.yaw));
        ImGui::Text("Pitch: %.1f deg", glm::degrees(camera.pitch));
        ImGui::Separator();
        ImGui::Text("Controls:");
        ImGui::BulletText("Arrow Keys: Rotate camera");
        ImGui::BulletText("W/S: Zoom in/out");
        ImGui::BulletText("B: Toggle biome overlay");
        ImGui::Separator();
        ImGui::Checkbox("Show Biome Overlay", &show_biome_overlay);
        if (water_map != nullptr) {
            ImGui::Text("Water Level: %.2f m", water_level);
        }
        if (biome_map != nullptr) {
            ImGui::Text("Biomes: Enabled");
        }
        if (environment_stats != nullptr) {
            static constexpr const char* kBiomeNames[] = {"Plains", "Forest", "Wetland", "Alpine"};
            static constexpr const char* kSpeciesNames[] = {"Grass", "Reed", "Lily", "Shrub", "Alpine Moss"};
            ImGui::Separator();
            ImGui::Text("Environment Stats");
            ImGui::Text("Soil Mean: %.2f", environment_stats->soil_mean);
            ImGui::Text("Land Fraction: %.0f%%", environment_stats->land_fraction * 100.0);
            ImGui::Text("Total Biomass: %.1f", environment_stats->total_biomass);
            ImGui::Text("Biome Biomass:");
            for (int i = 0; i < 4; ++i) {
                ImGui::BulletText("%s: %.1f", kBiomeNames[i], environment_stats->biome_biomass[i]);
            }
            ImGui::Text("Species Counts:");
            for (int i = 0; i < 5; ++i) {
                ImGui::BulletText("%s: %u", kSpeciesNames[i], environment_stats->species_counts[i]);
            }
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    destroy_terrain_mesh(terrain_mesh);
    if (water_mesh.vao != 0U) {
        destroy_terrain_mesh(water_mesh);
    }
    glDeleteProgram(program);
    glDeleteProgram(water_program);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}

