#include "renderer/renderer_app.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/scenario.h"
#include "evolution/sim/simulation_app.h"
#include "renderer/camera_controller.h"
#include "renderer/scene_extractor.h"

namespace evo = evolution::sim;

namespace evolution::client {

namespace {

struct Mesh {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};
};

struct PointBatch {
    GLuint vao{0};
    GLuint vbo{0};
    GLsizei count{0};
};

void destroy_mesh(Mesh& mesh) {
    if (mesh.ebo != 0U) {
        glDeleteBuffers(1, &mesh.ebo);
    }
    if (mesh.vbo != 0U) {
        glDeleteBuffers(1, &mesh.vbo);
    }
    if (mesh.vao != 0U) {
        glDeleteVertexArrays(1, &mesh.vao);
    }
    mesh = {};
}

void destroy_batch(PointBatch& batch) {
    if (batch.vbo != 0U) {
        glDeleteBuffers(1, &batch.vbo);
    }
    if (batch.vao != 0U) {
        glDeleteVertexArrays(1, &batch.vao);
    }
    batch = {};
}

[[nodiscard]] GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == 0) {
        GLchar info_log[1024];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        throw std::runtime_error(std::string("Shader compilation failed: ") + info_log);
    }
    return shader;
}

[[nodiscard]] GLuint link_program(GLuint vs, GLuint fs) {
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == 0) {
        GLchar info_log[1024];
        glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
        throw std::runtime_error(std::string("Program linking failed: ") + info_log);
    }
    return program;
}

[[nodiscard]] Mesh create_terrain_mesh_with_biomes(const evo::Terrain& terrain, const evo::BiomeMap* biome_map) {
    const int width = terrain.width();
    const int height = terrain.height_cells();
    const double cell_size = terrain.cell_size();

    constexpr glm::vec3 biome_colors[4] = {
        {0.6F, 0.8F, 0.4F},
        {0.2F, 0.5F, 0.2F},
        {0.4F, 0.6F, 0.5F},
        {0.7F, 0.7F, 0.8F},
    };

    std::vector<float> vertices;
    vertices.reserve(static_cast<std::size_t>(width * height * 9));

    for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
            const double world_x = static_cast<double>(x) * cell_size;
            const double world_z = static_cast<double>(z) * cell_size;
            const double world_y = terrain.height(world_x, world_z);
            const evo::Vec3 normal = terrain.normal(world_x, world_z);

            glm::vec3 color{0.5F, 0.8F, 0.4F};
            if (biome_map != nullptr) {
                const int idx = static_cast<int>(biome_map->sample(world_x, world_z));
                if (idx >= 0 && idx < 4) {
                    color = biome_colors[idx];
                }
            }

            vertices.push_back(static_cast<float>(world_x));
            vertices.push_back(static_cast<float>(world_y));
            vertices.push_back(static_cast<float>(world_z));
            vertices.push_back(static_cast<float>(normal.x));
            vertices.push_back(static_cast<float>(normal.y));
            vertices.push_back(static_cast<float>(normal.z));
            vertices.push_back(color.r);
            vertices.push_back(color.g);
            vertices.push_back(color.b);
        }
    }

    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>((width - 1) * (height - 1) * 6));

    for (int z = 0; z < height - 1; ++z) {
        for (int x = 0; x < width - 1; ++x) {
            const std::uint32_t i0 = static_cast<std::uint32_t>(z * width + x);
            const std::uint32_t i1 = static_cast<std::uint32_t>(z * width + x + 1);
            const std::uint32_t i2 = static_cast<std::uint32_t>((z + 1) * width + x);
            const std::uint32_t i3 = static_cast<std::uint32_t>((z + 1) * width + x + 1);
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    Mesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<const void*>(6 * sizeof(float)));

    glBindVertexArray(0);
    mesh.index_count = static_cast<GLsizei>(indices.size());
    return mesh;
}

[[nodiscard]] Mesh create_water_mesh(double level, double width, double height) {
    const std::vector<float> vertices = {
        0.0F, static_cast<float>(level), 0.0F,                0.0F, 1.0F, 0.0F, 0.0F, 0.0F,
        static_cast<float>(width), static_cast<float>(level), 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F,
        static_cast<float>(width), static_cast<float>(level), static_cast<float>(height), 0.0F, 1.0F, 0.0F, 1.0F, 1.0F,
        0.0F, static_cast<float>(level), static_cast<float>(height), 0.0F, 1.0F, 0.0F, 0.0F, 1.0F,
    };
    const std::vector<std::uint32_t> indices = {0, 1, 2, 0, 2, 3};

    Mesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<const void*>(6 * sizeof(float)));

    glBindVertexArray(0);
    mesh.index_count = static_cast<GLsizei>(indices.size());
    return mesh;
}

[[nodiscard]] PointBatch create_point_batch() {
    PointBatch batch{};
    glGenVertexArrays(1, &batch.vao);
    glGenBuffers(1, &batch.vbo);

    glBindVertexArray(batch.vao);
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    constexpr GLsizei stride = static_cast<GLsizei>(7 * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    glBindVertexArray(0);
    return batch;
}

void upload_point_batch(PointBatch& batch, const std::vector<float>& interleaved) {
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                 interleaved.data(),
                 GL_DYNAMIC_DRAW);
    batch.count = static_cast<GLsizei>(interleaved.size() / 7);
}

void glfw_error_callback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error (%d): %s\n", code, description);
}

[[nodiscard]] bool key_pressed_once(GLFWwindow* window, int key, bool& latch) {
    if (glfwGetKey(window, key) == GLFW_PRESS) {
        if (!latch) {
            latch = true;
            return true;
        }
    } else {
        latch = false;
    }
    return false;
}

[[nodiscard]] glm::vec3 color_for_species(std::uint32_t id) {
    static constexpr std::array<glm::vec3, 12> lut = {
        glm::vec3{0.95F, 0.45F, 0.25F}, glm::vec3{0.25F, 0.75F, 0.35F}, glm::vec3{0.25F, 0.55F, 0.95F},
        glm::vec3{0.95F, 0.85F, 0.30F}, glm::vec3{0.65F, 0.45F, 0.95F}, glm::vec3{0.30F, 0.85F, 0.85F},
        glm::vec3{0.95F, 0.60F, 0.75F}, glm::vec3{0.60F, 0.95F, 0.55F}, glm::vec3{0.85F, 0.75F, 0.55F},
        glm::vec3{0.55F, 0.70F, 0.95F}, glm::vec3{0.85F, 0.55F, 0.35F}, glm::vec3{0.35F, 0.85F, 0.65F},
    };
    return lut[id % lut.size()];
}

[[nodiscard]] std::uint8_t plant_species_palette_index(std::uint8_t id) {
    return static_cast<std::uint8_t>(id % 6);
}

[[nodiscard]] glm::vec3 color_for_plant(std::uint8_t id) {
    static constexpr std::array<glm::vec3, 6> lut = {
        glm::vec3{0.40F, 0.85F, 0.30F},
        glm::vec3{0.30F, 0.70F, 0.25F},
        glm::vec3{0.50F, 0.90F, 0.45F},
        glm::vec3{0.55F, 0.75F, 0.25F},
        glm::vec3{0.25F, 0.80F, 0.45F},
        glm::vec3{0.60F, 0.90F, 0.35F},
    };
    return lut[plant_species_palette_index(id)];
}

[[nodiscard]] bool project_to_screen(const glm::vec3& point,
                                     const glm::mat4& view,
                                     const glm::mat4& projection,
                                     int width,
                                     int height,
                                     glm::vec2& out) {
    const glm::vec4 clip = projection * view * glm::vec4(point, 1.0F);
    if (clip.w <= 0.0F) {
        return false;
    }
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.0F || ndc.z > 1.0F) {
        return false;
    }
    out.x = (ndc.x * 0.5F + 0.5F) * static_cast<float>(width);
    out.y = (1.0F - (ndc.y * 0.5F + 0.5F)) * static_cast<float>(height);
    return true;
}

struct PickState {
    entt::entity selected{entt::null};
    bool selected_is_plant{false};
};

void perform_selection_if_clicked(GLFWwindow* window,
                                  const RenderSnapshot& snapshot,
                                  const glm::mat4& view,
                                  const glm::mat4& projection,
                                  int width,
                                  int height,
                                  PickState& pick_state,
                                  bool& click_latch) {
    if (ImGui::GetIO().WantCaptureMouse) {
        click_latch = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        return;
    }

    const bool left_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (!left_down) {
        click_latch = false;
        return;
    }
    if (click_latch) {
        return;
    }
    click_latch = true;

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    const glm::vec2 mouse(static_cast<float>(mx), static_cast<float>(my));

    float best = 14.0F;
    entt::entity best_id = entt::null;
    bool best_is_plant = false;

    for (const auto& item : snapshot.creatures) {
        glm::vec2 screen{};
        if (!project_to_screen(item.position, view, projection, width, height, screen)) {
            continue;
        }
        const float d = glm::distance(screen, mouse);
        if (d < best) {
            best = d;
            best_id = item.id;
            best_is_plant = false;
        }
    }

    for (const auto& item : snapshot.plants) {
        glm::vec2 screen{};
        if (!project_to_screen(item.position, view, projection, width, height, screen)) {
            continue;
        }
        const float d = glm::distance(screen, mouse);
        if (d < best) {
            best = d;
            best_id = item.id;
            best_is_plant = true;
        }
    }

    if (best_id != entt::null) {
        pick_state.selected = best_id;
        pick_state.selected_is_plant = best_is_plant;
    }
}

}  // namespace

int RendererApp::run() {
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

    GLFWwindow* window = glfwCreateWindow(1600, 900, "Evolution Simulation Viewer", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "Failed to create GLFW window.\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
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

    constexpr const char* terrain_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
out vec3 vNormal;
out vec3 vColor;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    mat3 nm = transpose(inverse(mat3(uModel)));
    vNormal = normalize(nm * aNormal);
    vColor = aColor;
    gl_Position = uProjection * uView * wp;
})";

    constexpr const char* terrain_fs = R"(#version 330 core
in vec3 vNormal;
in vec3 vColor;
out vec4 FragColor;
uniform bool uUseVertexColor;
uniform vec3 uColor;
void main() {
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));
    float diff = max(dot(normalize(vNormal), lightDir), 0.0);
    vec3 base = (uUseVertexColor ? vColor : uColor) * (0.2 + 0.8 * diff);
    FragColor = vec4(base, 1.0);
})";

    constexpr const char* water_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
out vec3 vWorldPos;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    gl_Position = uProjection * uView * wp;
})";

    constexpr const char* water_fs = R"(#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;
uniform vec3 uViewPos;
uniform float uTime;
uniform float uWaterLevel;
void main() {
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    float fresnel = pow(1.0 - max(dot(viewDir, vec3(0.0,1.0,0.0)), 0.0), 2.0);
    float depthFactor = clamp((vWorldPos.y - uWaterLevel + 2.0) / 4.0, 0.0, 1.0);
    vec3 shallow = vec3(0.2, 0.6, 0.9);
    vec3 deep = vec3(0.05, 0.2, 0.4);
    vec3 water = mix(shallow, deep, depthFactor);
    float wave = sin(vWorldPos.x * 0.1 + uTime) * 0.02 + sin(vWorldPos.z * 0.15 + uTime * 1.2) * 0.02;
    water += vec3(wave * 0.1);
    vec3 color = mix(water, vec3(0.8, 0.9, 1.0), fresnel * 0.3);
    FragColor = vec4(color, 0.7);
})";

    constexpr const char* points_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in float aSize;
out vec3 vColor;
uniform mat4 uView;
uniform mat4 uProjection;
void main() {
    vec4 clip = uProjection * uView * vec4(aPos, 1.0);
    gl_Position = clip;
    gl_PointSize = aSize;
    vColor = aColor;
})";

    constexpr const char* points_fs = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(p, p);
    if (r2 > 1.0) {
        discard;
    }
    float edge = smoothstep(1.0, 0.8, 1.0 - r2);
    FragColor = vec4(vColor * edge, 1.0);
})";

    GLuint terrain_program = 0;
    GLuint water_program = 0;
    GLuint points_program = 0;
    try {
        terrain_program = link_program(compile_shader(GL_VERTEX_SHADER, terrain_vs),
                                       compile_shader(GL_FRAGMENT_SHADER, terrain_fs));
        water_program = link_program(compile_shader(GL_VERTEX_SHADER, water_vs),
                                     compile_shader(GL_FRAGMENT_SHADER, water_fs));
        points_program = link_program(compile_shader(GL_VERTEX_SHADER, points_vs),
                                      compile_shader(GL_FRAGMENT_SHADER, points_fs));
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "Shader error: %s\n", ex.what());
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    evo::SimulationApp app;
    evolution::genetics::GenomeStorage storage;
    evo::SimulationScenario scenario{};
    scenario.environment.terrain.width_cells = 256;
    scenario.environment.terrain.height_cells = 256;
    scenario.environment.terrain.cell_size = 2.0;
    scenario.environment.terrain.elevation_scale = 24.0;
    scenario.environment.terrain.seed = 2025;

    scenario.environment.soil.width_cells = 128;
    scenario.environment.soil.height_cells = 128;
    scenario.environment.soil.cell_size = 4.0;

    scenario.environment.plants.initial_count = 1000;
    scenario.initial_population = 180;
    scenario.enable_feeding_debug = false;
    scenario.enable_species_info_logs = false;
    scenario.enable_telemetry = false;
    scenario.stats_interval_s = 1.0;
    scenario.species_index_interval_s = 5.0;
    scenario.population.hard_cap = 3000;
    scenario.population.target_cap = 2200;
    scenario.population.max_carry_capacity = 1800;

    evo::setup_scenario(app, storage, scenario);

    auto* terrain_ptr = app.registry().ctx().find<evo::Terrain>();
    auto* biome_map_ptr = app.registry().ctx().find<evo::BiomeMap>();
    auto* water_map_ptr = app.registry().ctx().find<evo::WaterMap>();
    if (terrain_ptr == nullptr) {
        std::fprintf(stderr, "Terrain service missing.\n");
        return EXIT_FAILURE;
    }

    const evo::Terrain& terrain = *terrain_ptr;
    const float terrain_width = static_cast<float>(terrain.width() * terrain.cell_size());
    const float terrain_height = static_cast<float>(terrain.height_cells() * terrain.cell_size());
    const float center_x = terrain_width * 0.5F;
    const float center_z = terrain_height * 0.5F;
    const float avg_height = static_cast<float>((terrain.min_y() + terrain.max_y()) * 0.5);

    Mesh terrain_mesh = create_terrain_mesh_with_biomes(terrain, biome_map_ptr);

    Mesh water_mesh{};
    double water_level = 0.0;
    if (water_map_ptr != nullptr) {
        water_level = water_map_ptr->water_level();
        water_mesh = create_water_mesh(water_level, terrain_width, terrain_height);
    }

    PointBatch creatures_batch = create_point_batch();
    PointBatch plants_batch = create_point_batch();

    CameraController camera;
    camera.set_target(glm::vec3(center_x, avg_height, center_z));
    camera.set_mode(CameraMode::Free);
    camera.set_free_position(glm::vec3(center_x, avg_height + 25.0F, center_z + 70.0F));
    camera.set_free_angles(glm::radians(-90.0F), glm::radians(-20.0F));

    bool paused = false;
    bool step_once = false;
    float sim_speed = 0.5F;
    float max_draw_distance = 220.0F;
    float lod_far_distance = 90.0F;
    int max_sim_steps_per_frame = 3;
    int max_rendered_creatures = 2500;
    int max_rendered_plants = 8000;
    bool show_plants = true;
    bool show_creatures = true;
    bool show_biome_overlay = false;
    bool follow_selected = false;
    bool draw_terrain = true;
    bool draw_water = true;

    bool key_b_latch = false;
    bool key_space_latch = false;
    bool key_step_latch = false;
    bool mouse_click_latch = false;

    PickState pick_state{};

    const double fixed_dt = app.fixed_dt();
    double accumulator = 0.0;
    double last_time = glfwGetTime();

    std::size_t visible_creatures = 0;
    std::size_t culled_creatures = 0;
    std::size_t visible_plants = 0;
    std::size_t culled_plants = 0;
    std::size_t budget_dropped_creatures = 0;
    std::size_t budget_dropped_plants = 0;
    int sim_steps_this_frame = 0;

    int camera_mode_idx = 1;

    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const double dt_real = now - last_time;
        last_time = now;

        glfwPollEvents();

        if (key_pressed_once(window, GLFW_KEY_B, key_b_latch)) {
            show_biome_overlay = !show_biome_overlay;
        }
        if (key_pressed_once(window, GLFW_KEY_SPACE, key_space_latch)) {
            paused = !paused;
        }
        if (key_pressed_once(window, GLFW_KEY_N, key_step_latch)) {
            step_once = true;
        }

        if (!paused) {
            // Prevent spiral-of-death: never allow unbounded simulation catch-up.
            const double max_accumulator = fixed_dt * static_cast<double>(std::max(1, max_sim_steps_per_frame));
            accumulator = std::min(max_accumulator,
                                   accumulator + dt_real * std::max(0.05, static_cast<double>(sim_speed)));
            std::size_t steps = 0;
            while (accumulator >= fixed_dt && steps < static_cast<std::size_t>(std::max(1, max_sim_steps_per_frame))) {
                app.tick();
                accumulator -= fixed_dt;
                ++steps;
            }
            sim_steps_this_frame = static_cast<int>(steps);
        } else if (step_once) {
            app.tick();
            step_once = false;
            sim_steps_this_frame = 1;
        } else {
            sim_steps_this_frame = 0;
        }

        if (follow_selected) {
            camera.set_mode(CameraMode::Follow);
        } else if (camera_mode_idx == 1) {
            camera.set_mode(CameraMode::Free);
        } else {
            camera.set_mode(CameraMode::Orbit);
        }
        camera.update(window, static_cast<float>(dt_real), ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantCaptureMouse);

        RenderSnapshot snapshot = build_render_snapshot(app.registry(), app.simulation_time());

        if (follow_selected && pick_state.selected != entt::null) {
            bool found = false;
            if (pick_state.selected_is_plant) {
                for (const auto& p : snapshot.plants) {
                    if (p.id == pick_state.selected) {
                        camera.set_follow_target(p.position);
                        found = true;
                        break;
                    }
                }
            } else {
                for (const auto& c : snapshot.creatures) {
                    if (c.id == pick_state.selected) {
                        camera.set_follow_target(c.position);
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                pick_state.selected = entt::null;
                follow_selected = false;
            }
        }

        int fb_width = 0;
        int fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        const float aspect = fb_height > 0 ? static_cast<float>(fb_width) / static_cast<float>(fb_height) : 1.0F;

        const glm::mat4 view = camera.view_matrix();
        const glm::mat4 projection = glm::perspective(glm::radians(45.0F), aspect, 0.5F, 2500.0F);
        const glm::vec3 eye = camera.eye();

        perform_selection_if_clicked(window,
                                     snapshot,
                                     view,
                                     projection,
                                     fb_width,
                                     fb_height,
                                     pick_state,
                                     mouse_click_latch);

        std::vector<float> creature_points;
        std::vector<float> plant_points;
        creature_points.reserve(snapshot.creatures.size() * 7);
        plant_points.reserve(snapshot.plants.size() * 7);

        visible_creatures = 0;
        culled_creatures = 0;
        visible_plants = 0;
        culled_plants = 0;
        budget_dropped_creatures = 0;
        budget_dropped_plants = 0;

        for (const auto& c : snapshot.creatures) {
            const float dist = glm::distance(c.position, eye);
            if (dist > max_draw_distance) {
                ++culled_creatures;
                continue;
            }
            if (visible_creatures >= static_cast<std::size_t>(std::max(1, max_rendered_creatures))) {
                ++budget_dropped_creatures;
                continue;
            }
            ++visible_creatures;

            glm::vec3 color = c.is_carnivore ? glm::vec3(0.95F, 0.35F, 0.25F) : glm::vec3(0.30F, 0.75F, 0.35F);
            color = glm::mix(color * 0.4F, color_for_species(c.species_id), 0.5F);
            color *= (0.55F + 0.45F * c.energy_norm);
            color = glm::mix(color, glm::vec3(1.0F, 1.0F, 0.2F), c.speed_norm * 0.6F);
            float size = dist > lod_far_distance ? 3.0F : std::clamp(c.radius * 11.0F, 4.0F, 14.0F);

            if (pick_state.selected == c.id && !pick_state.selected_is_plant) {
                color = glm::vec3(1.0F, 1.0F, 0.25F);
                size *= 1.35F;
            }

            creature_points.push_back(c.position.x);
            creature_points.push_back(c.position.y + c.radius * 0.45F);
            creature_points.push_back(c.position.z);
            creature_points.push_back(color.r);
            creature_points.push_back(color.g);
            creature_points.push_back(color.b);
            creature_points.push_back(size);
        }

        for (const auto& p : snapshot.plants) {
            const float dist = glm::distance(p.position, eye);
            if (dist > max_draw_distance) {
                ++culled_plants;
                continue;
            }
            if (visible_plants >= static_cast<std::size_t>(std::max(1, max_rendered_plants))) {
                ++budget_dropped_plants;
                continue;
            }
            ++visible_plants;

            glm::vec3 color = color_for_plant(p.species_id);
            color *= (0.45F + 0.55F * p.energy_norm);
            float size = dist > lod_far_distance ? 2.5F : std::clamp(p.radius * 10.0F, 3.0F, 10.0F);

            if (pick_state.selected == p.id && pick_state.selected_is_plant) {
                color = glm::vec3(1.0F, 1.0F, 0.25F);
                size *= 1.35F;
            }

            plant_points.push_back(p.position.x);
            plant_points.push_back(p.position.y + p.radius * 0.25F);
            plant_points.push_back(p.position.z);
            plant_points.push_back(color.r);
            plant_points.push_back(color.g);
            plant_points.push_back(color.b);
            plant_points.push_back(size);
        }

        upload_point_batch(creatures_batch, creature_points);
        upload_point_batch(plants_batch, plant_points);

        glViewport(0, 0, fb_width, fb_height);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glClearColor(0.07F, 0.09F, 0.12F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 model(1.0F);

        if (draw_terrain) {
            glUseProgram(terrain_program);
            glUniformMatrix4fv(glGetUniformLocation(terrain_program, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(glGetUniformLocation(terrain_program, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(terrain_program, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform1i(glGetUniformLocation(terrain_program, "uUseVertexColor"), show_biome_overlay ? 1 : 0);
            glUniform3f(glGetUniformLocation(terrain_program, "uColor"), 0.45F, 0.78F, 0.38F);

            glBindVertexArray(terrain_mesh.vao);
            glDrawElements(GL_TRIANGLES, terrain_mesh.index_count, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }

        if (draw_water && water_mesh.vao != 0U) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            glUseProgram(water_program);
            glUniformMatrix4fv(glGetUniformLocation(water_program, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(glGetUniformLocation(water_program, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(water_program, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3fv(glGetUniformLocation(water_program, "uViewPos"), 1, glm::value_ptr(eye));
            glUniform1f(glGetUniformLocation(water_program, "uTime"), static_cast<float>(now));
            glUniform1f(glGetUniformLocation(water_program, "uWaterLevel"), static_cast<float>(water_level));

            glBindVertexArray(water_mesh.vao);
            glDrawElements(GL_TRIANGLES, water_mesh.index_count, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        glUseProgram(points_program);
        glUniformMatrix4fv(glGetUniformLocation(points_program, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(points_program, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));

        if (show_plants && plants_batch.count > 0) {
            glBindVertexArray(plants_batch.vao);
            glDrawArrays(GL_POINTS, 0, plants_batch.count);
            glBindVertexArray(0);
        }
        if (show_creatures && creatures_batch.count > 0) {
            glBindVertexArray(creatures_batch.vao);
            glDrawArrays(GL_POINTS, 0, creatures_batch.count);
            glBindVertexArray(0);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Simulation Viewer");
        ImGui::Text("t=%.2f s", snapshot.sim_time);
        ImGui::Text("Frame dt=%.3f ms (%.1f FPS)", dt_real * 1000.0, dt_real > 1e-6 ? 1.0 / dt_real : 0.0);
        ImGui::Separator();

        ImGui::Checkbox("Paused", &paused);
        ImGui::SameLine();
        if (ImGui::Button("Step")) {
            step_once = true;
        }
        ImGui::SliderFloat("Sim Speed", &sim_speed, 0.1F, 5.0F, "%.2fx");
        ImGui::SliderInt("Max Sim Steps/Frame", &max_sim_steps_per_frame, 1, 8);

        camera_mode_idx = follow_selected ? 2 : camera_mode_idx;
        const char* camera_modes[] = {"Orbit", "Free", "Follow"};
        if (ImGui::Combo("Camera Mode", &camera_mode_idx, camera_modes, 3)) {
            if (camera_mode_idx == 2 && pick_state.selected == entt::null) {
                camera_mode_idx = 0;
            }
            follow_selected = camera_mode_idx == 2;
            // Camera mode is applied at frame update stage.
        }

        if (ImGui::Button("Follow Selected")) {
            if (pick_state.selected != entt::null) {
                follow_selected = true;
                camera_mode_idx = 2;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Selection")) {
            pick_state = {};
            follow_selected = false;
            camera_mode_idx = 0;
        }

        ImGui::SliderFloat("Max Draw Distance", &max_draw_distance, 60.0F, 800.0F, "%.0f m");
        ImGui::SliderFloat("LOD Far Distance", &lod_far_distance, 20.0F, 400.0F, "%.0f m");
        ImGui::SliderInt("Max Rendered Creatures", &max_rendered_creatures, 100, 20000);
        ImGui::SliderInt("Max Rendered Plants", &max_rendered_plants, 500, 50000);
        if (lod_far_distance > max_draw_distance) {
            lod_far_distance = max_draw_distance;
        }

        ImGui::Checkbox("Show Biome Overlay", &show_biome_overlay);
        ImGui::Checkbox("Draw Terrain", &draw_terrain);
        ImGui::Checkbox("Draw Water", &draw_water);
        ImGui::Checkbox("Show Plants", &show_plants);
        ImGui::Checkbox("Show Creatures", &show_creatures);

        ImGui::Separator();
        ImGui::Text("Camera: dist=%.1f yaw=%.1f pitch=%.1f", camera.distance(), glm::degrees(camera.yaw()), glm::degrees(camera.pitch()));
        ImGui::Text("Sim steps this frame: %d", sim_steps_this_frame);
        ImGui::Text("Entities: creatures %zu (vis %zu, culled %zu)", snapshot.creatures.size(), visible_creatures, culled_creatures);
        ImGui::Text("Entities: plants %zu (vis %zu, culled %zu)", snapshot.plants.size(), visible_plants, culled_plants);
        ImGui::Text("Budget drops: creatures %zu, plants %zu", budget_dropped_creatures, budget_dropped_plants);
        ImGui::Text("Draw calls: %d", (show_plants ? 1 : 0) + (show_creatures ? 1 : 0) + 2);

        if (pick_state.selected != entt::null) {
            ImGui::Separator();
            ImGui::Text("Selected: %u (%s)", static_cast<unsigned>(entt::to_integral(pick_state.selected)), pick_state.selected_is_plant ? "plant" : "creature");
            if (pick_state.selected_is_plant) {
                auto it = std::find_if(snapshot.plants.begin(), snapshot.plants.end(), [&](const PlantRenderItem& p) { return p.id == pick_state.selected; });
                if (it != snapshot.plants.end()) {
                    ImGui::Text("Species=%u energy=%.2f", static_cast<unsigned>(it->species_id), it->energy_norm);
                    ImGui::Text("Pos=(%.1f, %.1f, %.1f)", it->position.x, it->position.y, it->position.z);
                }
            } else {
                auto it = std::find_if(snapshot.creatures.begin(), snapshot.creatures.end(), [&](const CreatureRenderItem& c) { return c.id == pick_state.selected; });
                if (it != snapshot.creatures.end()) {
                    ImGui::Text("Species(hash)=%u", it->species_id);
                    ImGui::Text("Diet=%s energy=%.2f", it->is_carnivore ? "carnivore" : "herbivore", it->energy_norm);
                    ImGui::Text("Pos=(%.1f, %.1f, %.1f)", it->position.x, it->position.y, it->position.z);
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Controls Orbit: Arrows rotate, WASD pan, Q/E zoom");
        ImGui::Text("Controls Free: RMB look + WASD move + Q/E vertical");
        ImGui::Text("Hotkeys: Space pause, N step, B biome overlay, click select");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    destroy_mesh(terrain_mesh);
    destroy_mesh(water_mesh);
    destroy_batch(creatures_batch);
    destroy_batch(plants_batch);

    glDeleteProgram(terrain_program);
    glDeleteProgram(water_program);
    glDeleteProgram(points_program);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}

}  // namespace evolution::client
