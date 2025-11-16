#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include <entt/entt.hpp>

#include "evolution/client/terrain_textures.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/scenario.h"
#include "evolution/sim/simulation_app.h"

namespace evo = evolution::sim;
namespace genetics = evolution::genetics;
namespace client = evolution::client;

namespace {

[[nodiscard]] glm::vec3 to_glm(const evo::Vec3& v) {
    return glm::vec3(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
}

struct OrbitCamera {
    glm::vec3 target{0.0F, 10.0F, 0.0F};
    float distance{85.0F};
    float yaw{0.0F};
    float pitch{0.65F};
};

[[nodiscard]] glm::vec3 compute_camera_position(const OrbitCamera& camera) {
    const float cp = std::cos(camera.pitch);
    const float sp = std::sin(camera.pitch);
    const float cy = std::cos(camera.yaw);
    const float sy = std::sin(camera.yaw);
    return camera.target + glm::vec3(camera.distance * cp * cy, camera.distance * sp,
                                     camera.distance * cp * sy);
}

void update_camera(GLFWwindow* window, OrbitCamera& camera, float delta_seconds) {
    const float rotation_speed = glm::radians(70.0F);
    const float zoom_speed = 20.0F;
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
    camera.distance = std::clamp(camera.distance, 15.0F, 400.0F);
}

struct TimeControls {
    double time_scale{1.0};
    bool paused{false};
    bool single_step{false};
};

struct RenderToggles {
    bool show_biome_overlay{false};
    bool show_water{true};
    bool show_agents{true};
    bool show_plants{true};
    bool show_energy_overlay{false};
    float energy_overlay_strength{0.6F};
    bool enable_triplanar{true};
    bool enable_normal_maps{true};
    bool enable_stochastic{false};
    float cliff_threshold{0.4F};
    float stochastic_intensity{0.5F};
};

struct KeyLatch {
    bool pause{false};
    bool step{false};
    bool biome_overlay{false};
};

struct Mesh {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};
};

struct InstanceBuffer {
    GLuint buffer{0};
    std::size_t capacity{0};
};

struct InstancedGeometry {
    Mesh mesh{};
    InstanceBuffer instances{};
};

struct InstanceData {
    glm::mat4 model{1.0F};
    glm::vec4 color{1.0F};
    float debug_scalar{0.0F};
    float padding[3]{};
};

struct TransformHistory {
    struct Samples {
        glm::vec3 previous{0.0F};
        glm::vec3 current{0.0F};
    };

    std::unordered_map<entt::entity, Samples> entries{};

    void initialize(const entt::registry& registry) {
        entries.clear();
        auto view = registry.view<const evo::TransformComponent>();
        entries.reserve(static_cast<std::size_t>(view.size()));
        for (auto entity : view) {
            const auto& transform = view.get<const evo::TransformComponent>(entity);
            const glm::vec3 pos = to_glm(transform.position);
            entries.emplace(entity, Samples{pos, pos});
        }
    }

    void capture(const entt::registry& registry) {
        std::unordered_map<entt::entity, Samples> next;
        auto view = registry.view<const evo::TransformComponent>();
        next.reserve(static_cast<std::size_t>(view.size()));
        for (auto entity : view) {
            const auto& transform = view.get<const evo::TransformComponent>(entity);
            const glm::vec3 pos = to_glm(transform.position);
            Samples samples{};
            if (const auto it = entries.find(entity); it != entries.end()) {
                samples.previous = it->second.current;
            } else {
                samples.previous = pos;
            }
            samples.current = pos;
            next.emplace(entity, samples);
        }
        entries.swap(next);
    }

    [[nodiscard]] glm::vec3 sample(entt::entity entity, double alpha, const evo::Vec3& fallback) const {
        const auto it = entries.find(entity);
        if (it == entries.end()) {
            return to_glm(fallback);
        }
        const float t = std::clamp(static_cast<float>(alpha), 0.0F, 1.0F);
        return glm::mix(it->second.previous, it->second.current, t);
    }
};

void glfw_error_callback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error (%d): %s\n", code, description);
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

[[nodiscard]] GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == 0) {
        GLchar info_log[1024];
        glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
        throw std::runtime_error(std::string("Program linking failed: ") + info_log);
    }
    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}

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

void destroy_instance_buffer(InstanceBuffer& buffer) {
    if (buffer.buffer != 0U) {
        glDeleteBuffers(1, &buffer.buffer);
        buffer.buffer = 0;
    }
    buffer.capacity = 0;
}

struct GatherResult {
    std::size_t total{0};
    std::size_t visible{0};
};

[[nodiscard]] Mesh create_terrain_mesh_with_biomes(const evo::Terrain& terrain,
                                                   const evo::BiomeMap* biome_map) {
    const int width = terrain.width();
    const int height = terrain.height_cells();
    const double cell_size = terrain.cell_size();

    constexpr glm::vec3 biome_colors[4] = {
        glm::vec3(0.60F, 0.82F, 0.42F),
        glm::vec3(0.20F, 0.55F, 0.26F),
        glm::vec3(0.30F, 0.70F, 0.62F),
        glm::vec3(0.75F, 0.75F, 0.80F),
    };

    std::vector<float> vertices;
    vertices.reserve(static_cast<std::size_t>(width * height * 9));

    for (int iz = 0; iz < height; ++iz) {
        for (int ix = 0; ix < width; ++ix) {
            const double x = static_cast<double>(ix) * cell_size;
            const double z = static_cast<double>(iz) * cell_size;
            const double y = terrain.height(x, z);
            const evo::Vec3 normal = terrain.normal(x, z);

            glm::vec3 color{0.5F, 0.8F, 0.4F};
            if (biome_map != nullptr) {
                const int biome_idx = static_cast<int>(biome_map->sample(x, z));
                if (biome_idx >= 0 && biome_idx < 4) {
                    color = biome_colors[biome_idx];
                }
            }

            vertices.push_back(static_cast<float>(x));
            vertices.push_back(static_cast<float>(y));
            vertices.push_back(static_cast<float>(z));
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

    Mesh mesh{};
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

[[nodiscard]] Mesh create_water_mesh(double water_level, double width, double height) {
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

[[nodiscard]] Mesh create_uv_sphere_mesh(int stacks = 18, int slices = 32) {
    Mesh mesh{};
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;

    for (int stack = 0; stack <= stacks; ++stack) {
        const float v = static_cast<float>(stack) / static_cast<float>(stacks);
        const float phi = glm::pi<float>() * v;
        const float sin_phi = std::sin(phi);
        const float cos_phi = std::cos(phi);
        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float theta = glm::two_pi<float>() * u;
            const float sin_theta = std::sin(theta);
            const float cos_theta = std::cos(theta);
            const glm::vec3 normal{cos_theta * sin_phi, cos_phi, sin_theta * sin_phi};
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
        }
    }

    for (int stack = 0; stack < stacks; ++stack) {
        for (int slice = 0; slice < slices; ++slice) {
            const std::uint32_t first = static_cast<std::uint32_t>(stack * (slices + 1) + slice);
            const std::uint32_t second = first + static_cast<std::uint32_t>(slices + 1);
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glBindVertexArray(0);

    mesh.index_count = static_cast<GLsizei>(indices.size());
    return mesh;
}

[[nodiscard]] Mesh create_cylinder_mesh(int slices = 24) {
    Mesh mesh{};
    const float half_height = 0.5F;
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;

    for (int i = 0; i <= slices; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(slices);
        const float angle = glm::two_pi<float>() * t;
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        vertices.push_back(x);
        vertices.push_back(-half_height);
        vertices.push_back(z);
        vertices.push_back(x);
        vertices.push_back(0.0F);
        vertices.push_back(z);
        vertices.push_back(x);
        vertices.push_back(half_height);
        vertices.push_back(z);
        vertices.push_back(x);
        vertices.push_back(0.0F);
        vertices.push_back(z);
    }

    const std::uint32_t top_center_index = static_cast<std::uint32_t>(vertices.size() / 6);
    vertices.insert(vertices.end(), {0.0F, half_height, 0.0F, 0.0F, 1.0F, 0.0F});
    const std::uint32_t top_ring_start = static_cast<std::uint32_t>(vertices.size() / 6);
    for (int i = 0; i <= slices; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(slices);
        const float angle = glm::two_pi<float>() * t;
        vertices.push_back(std::cos(angle));
        vertices.push_back(half_height);
        vertices.push_back(std::sin(angle));
        vertices.push_back(0.0F);
        vertices.push_back(1.0F);
        vertices.push_back(0.0F);
    }

    const std::uint32_t bottom_center_index = static_cast<std::uint32_t>(vertices.size() / 6);
    vertices.insert(vertices.end(), {0.0F, -half_height, 0.0F, 0.0F, -1.0F, 0.0F});
    const std::uint32_t bottom_ring_start = static_cast<std::uint32_t>(vertices.size() / 6);
    for (int i = 0; i <= slices; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(slices);
        const float angle = glm::two_pi<float>() * t;
        vertices.push_back(std::cos(angle));
        vertices.push_back(-half_height);
        vertices.push_back(std::sin(angle));
        vertices.push_back(0.0F);
        vertices.push_back(-1.0F);
        vertices.push_back(0.0F);
    }

    for (int i = 0; i < slices; ++i) {
        const std::uint32_t bottom0 = static_cast<std::uint32_t>(i * 2);
        const std::uint32_t top0 = bottom0 + 1;
        const std::uint32_t bottom1 = static_cast<std::uint32_t>((i + 1) * 2);
        const std::uint32_t top1 = bottom1 + 1;
        indices.push_back(bottom0);
        indices.push_back(top0);
        indices.push_back(bottom1);
        indices.push_back(bottom1);
        indices.push_back(top0);
        indices.push_back(top1);
    }

    for (int i = 0; i < slices; ++i) {
        const std::uint32_t ring0 = top_ring_start + static_cast<std::uint32_t>(i);
        const std::uint32_t ring1 = ring0 + 1;
        indices.push_back(top_center_index);
        indices.push_back(ring1);
        indices.push_back(ring0);

        const std::uint32_t bring0 = bottom_ring_start + static_cast<std::uint32_t>(i);
        const std::uint32_t bring1 = bring0 + 1;
        indices.push_back(bottom_center_index);
        indices.push_back(bring0);
        indices.push_back(bring1);
    }

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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glBindVertexArray(0);

    mesh.index_count = static_cast<GLsizei>(indices.size());
    return mesh;
}

[[nodiscard]] InstanceBuffer create_instance_buffer(GLuint vao) {
    InstanceBuffer buffer{};
    glGenBuffers(1, &buffer.buffer);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer.buffer);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    constexpr GLsizei stride = sizeof(InstanceData);
    const auto offset = [](std::size_t field_offset) -> const void* {
        return reinterpret_cast<const void*>(field_offset);
    };

    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, stride,
                              offset(offsetof(InstanceData, model) + sizeof(glm::vec4) * i));
        glVertexAttribDivisor(3 + i, 1);
    }

    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, stride, offset(offsetof(InstanceData, color)));
    glVertexAttribDivisor(7, 1);

    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, stride,
                          offset(offsetof(InstanceData, debug_scalar)));
    glVertexAttribDivisor(8, 1);

    glBindVertexArray(0);
    return buffer;
}

void ensure_instance_capacity(InstanceBuffer& buffer, std::size_t count) {
    if (count <= buffer.capacity) {
        return;
    }
    const std::size_t new_capacity = std::max(buffer.capacity * 2, std::max<std::size_t>(64, count));
    glBindBuffer(GL_ARRAY_BUFFER, buffer.buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(new_capacity * sizeof(InstanceData)),
                 nullptr, GL_DYNAMIC_DRAW);
    buffer.capacity = new_capacity;
}

void upload_instance_data(InstanceBuffer& buffer, const std::vector<InstanceData>& instances) {
    if (instances.empty()) {
        return;
    }
    ensure_instance_capacity(buffer, instances.size());
    glBindBuffer(GL_ARRAY_BUFFER, buffer.buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(instances.size() * sizeof(InstanceData)),
                    instances.data());
}

glm::vec3 energy_to_color(float ratio) {
    const glm::vec3 low{0.1F, 0.35F, 0.95F};
    const glm::vec3 mid{0.2F, 0.85F, 0.45F};
    const glm::vec3 high{0.95F, 0.32F, 0.18F};
    if (ratio < 0.5F) {
        return glm::mix(low, mid, ratio * 2.0F);
    }
    return glm::mix(mid, high, (ratio - 0.5F) * 2.0F);
}

glm::vec3 plant_species_color(std::uint8_t species_id) {
    constexpr std::array<glm::vec3, 6> palette{
        glm::vec3(0.55F, 0.80F, 0.25F), glm::vec3(0.85F, 0.65F, 0.15F),
        glm::vec3(0.95F, 0.35F, 0.35F), glm::vec3(0.45F, 0.65F, 0.90F),
        glm::vec3(0.90F, 0.45F, 0.80F), glm::vec3(0.40F, 0.90F, 0.80F)};
    return palette[species_id % palette.size()];
}

glm::vec3 collider_scale(const evo::ColliderComponent& collider, double size_scale) {
    const float scale = static_cast<float>(size_scale);
    switch (collider.type) {
        case evo::ShapeType::Sphere: {
            const float radius = static_cast<float>(collider.sphere.radius) * scale;
            return glm::vec3(radius);
        }
        case evo::ShapeType::CapsuleY: {
            const float radius = static_cast<float>(collider.capsule.radius) * scale;
            const float height =
                static_cast<float>(collider.capsule.half_height + collider.capsule.radius) * scale;
            return glm::vec3(radius, std::max(height, radius * 1.2F), radius);
        }
        case evo::ShapeType::Aabb:
        default:
            return glm::vec3(static_cast<float>(collider.aabb.half_extents.x) * scale,
                             static_cast<float>(collider.aabb.half_extents.y) * scale,
                             static_cast<float>(collider.aabb.half_extents.z) * scale);
    }
}

GatherResult build_agent_instances(const entt::registry& registry,
                                   const TransformHistory& history,
                                   double alpha,
                                   std::vector<InstanceData>& out_instances) {
    auto view = registry.view<const evo::TransformComponent,
                              const evo::ColliderComponent,
                              const evo::MetabolismComponent,
                              const evo::LifecycleComponent,
                              const evo::HerbivoreTag>();
    GatherResult stats{};
    stats.total = view.size_hint();
    out_instances.clear();
    out_instances.reserve(stats.total);

    for (auto entity : view) {
        const auto& transform = view.get<const evo::TransformComponent>(entity);
        const auto& collider = view.get<const evo::ColliderComponent>(entity);
        const auto& metabolism = view.get<const evo::MetabolismComponent>(entity);
        const auto& lifecycle = view.get<const evo::LifecycleComponent>(entity);

        InstanceData instance{};
        const glm::vec3 position = history.sample(entity, alpha, transform.position);
        const glm::vec3 scale = collider_scale(collider, lifecycle.size_scale);
        instance.model =
            glm::scale(glm::translate(glm::mat4(1.0F), position), glm::max(scale, glm::vec3(0.05F)));

        const float ratio =
            static_cast<float>(std::clamp(metabolism.energy / std::max(1.0, metabolism.max_energy),
                                          0.0, 1.0));
        instance.color = glm::vec4(energy_to_color(ratio), 1.0F);
        instance.debug_scalar = ratio;
        out_instances.push_back(instance);
    }

    stats.visible = out_instances.size();
    return stats;
}

GatherResult build_plant_instances(const entt::registry& registry,
                                   const TransformHistory& history,
                                   double alpha,
                                   std::vector<InstanceData>& out_instances) {
    auto view = registry.view<const evo::TransformComponent, const evo::PlantComponent>();
    GatherResult stats{};
    stats.total = view.size_hint();
    out_instances.clear();
    out_instances.reserve(stats.total);

    for (auto entity : view) {
        const auto& transform = view.get<const evo::TransformComponent>(entity);
        const auto& plant = view.get<const evo::PlantComponent>(entity);
        if (!plant.alive) {
            continue;
        }

        InstanceData instance{};
        const glm::vec3 position = history.sample(entity, alpha, transform.position);
        const float energy_ratio =
            static_cast<float>(std::clamp(plant.energy / std::max(1.0, plant.max_energy), 0.0, 1.0));
        const float radius = std::max(0.2F, static_cast<float>(plant.radius));
        const float height = radius * (1.2F + energy_ratio * 2.5F);

        instance.model = glm::scale(glm::translate(glm::mat4(1.0F), position),
                                    glm::vec3(radius, height, radius));
        instance.color = glm::vec4(plant_species_color(plant.species_id), 0.95F);
        instance.debug_scalar = energy_ratio;
        out_instances.push_back(instance);
    }

    stats.visible = out_instances.size();
    return stats;
}

void handle_hotkeys(GLFWwindow* window, TimeControls& time_controls,
                    RenderToggles& toggles, KeyLatch& latch) {
    const auto handle_press = [](bool is_pressed, bool& key_latch, auto&& action) {
        if (is_pressed) {
            if (!key_latch) {
                action();
                key_latch = true;
            }
        } else {
            key_latch = false;
        }
    };

    handle_press(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS, latch.pause, [&] {
        time_controls.paused = !time_controls.paused;
    });

    handle_press(glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS, latch.step, [&] {
        time_controls.single_step = true;
    });

    handle_press(glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS, latch.biome_overlay, [&] {
        toggles.show_biome_overlay = !toggles.show_biome_overlay;
    });
}

struct FrameTimings {
    double frame_ms{0.0};
    double sim_ms{0.0};
    double render_ms{0.0};
    std::size_t ticks{0};
};

void draw_simulation_hud(TimeControls& time_controls,
                         RenderToggles& toggles,
                         const FrameTimings& timings,
                         double sim_time,
                         const GatherResult& agent_stats,
                         const GatherResult& plant_stats,
                         const evo::EnvironmentStats* env_stats) {
    ImGui::Begin("Simulation Viewer");
    ImGui::Text("Frame: %.2f ms (%.1f FPS)", timings.frame_ms,
                (timings.frame_ms > 0.0) ? 1000.0 / timings.frame_ms : 0.0);
    ImGui::Text("Simulation: %.2f ms, Render: %.2f ms, Ticks: %zu", timings.sim_ms, timings.render_ms,
                timings.ticks);
    ImGui::Separator();
    ImGui::Text("Sim time: %.1f s", sim_time);
    if (ImGui::Checkbox("Paused", &time_controls.paused)) {
        if (!time_controls.paused) {
            time_controls.single_step = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Single Step")) {
        time_controls.single_step = true;
        time_controls.paused = true;
    }
    float time_scale = static_cast<float>(time_controls.time_scale);
    if (ImGui::SliderFloat("Time Scale", &time_scale, 0.1F, 4.0F, "%.2fx")) {
        time_controls.time_scale = time_scale;
    }

    ImGui::Separator();
    ImGui::Checkbox("Show Biomes", &toggles.show_biome_overlay);
    ImGui::Checkbox("Show Water", &toggles.show_water);
    ImGui::Checkbox("Show Agents", &toggles.show_agents);
    ImGui::Checkbox("Show Plants", &toggles.show_plants);
    ImGui::Checkbox("Energy Overlay", &toggles.show_energy_overlay);
    if (toggles.show_energy_overlay) {
        ImGui::SliderFloat("Overlay Strength", &toggles.energy_overlay_strength, 0.0F, 1.0F);
    }

    ImGui::Separator();
    ImGui::Text("Terrain Shading");
    ImGui::Checkbox("Triplanar Mapping", &toggles.enable_triplanar);
    if (toggles.enable_triplanar) {
        ImGui::SliderFloat("Cliff Threshold", &toggles.cliff_threshold, 0.1F, 0.9F);
    }
    ImGui::Checkbox("Normal Maps", &toggles.enable_normal_maps);
    ImGui::Checkbox("Stochastic Sampling", &toggles.enable_stochastic);
    if (toggles.enable_stochastic) {
        ImGui::SliderFloat("Stochastic Intensity", &toggles.stochastic_intensity, 0.0F, 1.0F);
    }

    ImGui::Separator();
    ImGui::Text("Agents: %zu", agent_stats.visible);
    ImGui::Text("Plants: %zu / %zu", plant_stats.visible, plant_stats.total);

    if (env_stats != nullptr) {
        static constexpr const char* kBiomeNames[] = {"Plains", "Forest", "Wetland", "Alpine"};
        static constexpr const char* kSpeciesNames[] = {"Grass", "Reed", "Lily", "Shrub", "Moss"};
        ImGui::Separator();
        ImGui::Text("Environment Stats");
        ImGui::Text("Total Biomass: %.1f", env_stats->total_biomass);
        ImGui::Text("Soil Mean: %.2f", env_stats->soil_mean);
        ImGui::Text("Land Fraction: %.0f%%", env_stats->land_fraction * 100.0);
        if (ImGui::TreeNode("Biome Biomass")) {
            for (int i = 0; i < 4; ++i) {
                ImGui::BulletText("%s: %.1f", kBiomeNames[i], env_stats->biome_biomass[i]);
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Species Counts")) {
            for (int i = 0; i < 5; ++i) {
                ImGui::BulletText("%s: %u", kSpeciesNames[i], env_stats->species_counts[i]);
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

}  // namespace

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() != GLFW_TRUE) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window =
        glfwCreateWindow(1600, 900, "Evolution Simulation Viewer", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    const char* terrain_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    mat3 normalMatrix = mat3(transpose(inverse(uModel)));
    vNormal = normalize(normalMatrix * aNormal);
    vColor = aColor;
    gl_Position = uProjection * uView * worldPos;
}
)";

    const char* terrain_fs = R"(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;

uniform sampler2DArray uAlbedoArray;
uniform sampler2DArray uNormalArray;
uniform vec3 uFallbackColor;
uniform bool uUseVertexColor;
uniform bool uEnableTriplanar;
uniform bool uEnableNormalMaps;
uniform bool uEnableStochastic;
uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform float uCliffThreshold;
uniform float uStochasticIntensity;
uniform float uTerrainMinY;
uniform float uTerrainMaxY;
uniform float uWaterLevel;
uniform float uGlobalSeed;

out vec4 FragColor;

// Deterministic hash function for stochastic sampling
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float hash3d(vec3 p) {
    return hash(p.xy + hash(vec2(p.z, p.z + 1.0)));
}

// Stochastic texture sampling with deterministic offsets
vec4 sampleStochastic(sampler2DArray tex, vec3 coord, float intensity) {
    if (intensity <= 0.0) {
        return texture(tex, coord);
    }
    
    vec2 tileCoord = floor(coord.xy * 8.0);
    vec2 offset = vec2(
        hash(tileCoord + vec2(0.0, 0.0)) - 0.5,
        hash(tileCoord + vec2(1.0, 0.0)) - 0.5
    ) * intensity * 0.1;
    
    vec4 sample1 = texture(tex, coord + vec3(offset, 0.0));
    vec4 sample2 = texture(tex, coord + vec3(-offset.yx, 0.0));
    
    return mix(sample1, sample2, 0.5);
}

// Triplanar texture sampling
vec4 sampleTriplanar(sampler2DArray tex, vec3 worldPos, vec3 normal, int materialIndex, float scale) {
    vec3 absNormal = abs(normal);
    float totalWeight = absNormal.x + absNormal.y + absNormal.z;
    absNormal /= totalWeight;
    
    vec4 sampleX = texture(tex, vec3(worldPos.zy * scale, float(materialIndex)));
    vec4 sampleY = texture(tex, vec3(worldPos.xz * scale, float(materialIndex)));
    vec4 sampleZ = texture(tex, vec3(worldPos.xy * scale, float(materialIndex)));
    
    return sampleX * absNormal.x + sampleY * absNormal.y + sampleZ * absNormal.z;
}

// Sample normal map with triplanar support
vec3 sampleNormalTriplanar(sampler2DArray tex, vec3 worldPos, vec3 normal, int materialIndex, float scale) {
    vec3 absNormal = abs(normal);
    float totalWeight = absNormal.x + absNormal.y + absNormal.z;
    absNormal /= totalWeight;
    
    vec3 sampleX = texture(tex, vec3(worldPos.zy * scale, float(materialIndex))).rgb * 2.0 - 1.0;
    vec3 sampleY = texture(tex, vec3(worldPos.xz * scale, float(materialIndex))).rgb * 2.0 - 1.0;
    vec3 sampleZ = texture(tex, vec3(worldPos.xy * scale, float(materialIndex))).rgb * 2.0 - 1.0;
    
    // Transform to world space for each projection
    vec3 normalX = normalize(vec3(0.0, sampleX.y, sampleX.x));
    vec3 normalY = normalize(vec3(sampleY.x, 0.0, sampleY.y));
    vec3 normalZ = normalize(vec3(sampleZ.x, sampleZ.y, 0.0));
    
    return normalize(normalX * absNormal.x + normalY * absNormal.y + normalZ * absNormal.z);
}

// Macro variation noise
float macroNoise(vec2 p) {
    return fract(sin(dot(p + vec2(uGlobalSeed), vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec3 worldPos = vWorldPos;
    vec3 geomNormal = normalize(vNormal);
    
    // Compute steepness for triplanar blending
    float steepness = 1.0 - dot(geomNormal, vec3(0.0, 1.0, 0.0));
    float useTriplanar = uEnableTriplanar ? smoothstep(uCliffThreshold - 0.1, uCliffThreshold + 0.1, steepness) : 0.0;
    
    // Material selection based on height, slope, and biome (simplified)
    // In a full implementation, this would sample BiomeMap and WaterMap
    int materialIndex = 0; // Default to grass
    
    float heightNormalized = (worldPos.y - uTerrainMinY) / max(uTerrainMaxY - uTerrainMinY, 0.001);
    
    // Height-based material selection
    if (heightNormalized > 0.7) {
        materialIndex = 4; // Snow
    } else if (steepness > 0.5) {
        materialIndex = 2; // Rock
    } else if (worldPos.y < uWaterLevel + 2.0) {
        materialIndex = 3; // Sand
    } else {
        materialIndex = 0; // Grass
    }
    
    // Macro variation
    vec2 macroCoord = worldPos.xz * 0.01;
    float macroVariation = macroNoise(macroCoord) * 0.1 + 0.95;
    
    // Texture scale
    float texScale = 0.1;
    
    // Sample albedo
    vec4 albedo;
    if (useTriplanar > 0.5) {
        albedo = sampleTriplanar(uAlbedoArray, worldPos, geomNormal, materialIndex, texScale);
    } else {
        vec3 texCoord = vec3(worldPos.xz * texScale, float(materialIndex));
        if (uEnableStochastic && materialIndex == 0) {
            albedo = sampleStochastic(uAlbedoArray, texCoord, uStochasticIntensity);
        } else {
            albedo = texture(uAlbedoArray, texCoord);
        }
    }
    
    // Apply macro variation
    albedo.rgb *= macroVariation;
    
    // Sample normal map
    vec3 finalNormal = geomNormal;
    if (uEnableNormalMaps) {
        vec3 normalMapSample;
        if (useTriplanar > 0.5) {
            normalMapSample = sampleNormalTriplanar(uNormalArray, worldPos, geomNormal, materialIndex, texScale);
        } else {
            vec3 texCoord = vec3(worldPos.xz * texScale, float(materialIndex));
            normalMapSample = texture(uNormalArray, texCoord).rgb * 2.0 - 1.0;
            // Simple tangent space to world space (approximate)
            vec3 tangent = normalize(vec3(1.0, 0.0, geomNormal.x));
            vec3 bitangent = cross(geomNormal, tangent);
            mat3 tbn = mat3(tangent, bitangent, geomNormal);
            normalMapSample = normalize(tbn * normalMapSample);
        }
        finalNormal = normalize(mix(geomNormal, normalMapSample, 0.7));
    }
    
    // Fallback to vertex color if enabled
    if (uUseVertexColor) {
        albedo.rgb = vColor;
    }
    
    // Lighting
    vec3 lightDir = normalize(uLightDir);
    float diff = max(dot(finalNormal, lightDir), 0.0);
    vec3 lit = albedo.rgb * (0.2 + 0.8 * diff);
    
    FragColor = vec4(lit, 1.0);
}
)";

    const char* water_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = uProjection * uView * worldPos;
}
)";

    const char* water_fs = R"(#version 330 core
in vec3 vWorldPos;
uniform vec3 uViewPos;
uniform float uTime;
uniform float uWaterLevel;

out vec4 FragColor;

void main() {
    vec3 view = normalize(uViewPos - vWorldPos);
    vec3 normal = vec3(0.0, 1.0, 0.0);
    float fresnel = pow(1.0 - max(dot(view, normal), 0.0), 3.0);
    float depth_factor = clamp((uWaterLevel - vWorldPos.y + 3.0) / 6.0, 0.0, 1.0);
    vec3 shallow = vec3(0.2, 0.7, 0.9);
    vec3 deep = vec3(0.0, 0.2, 0.45);
    vec3 color = mix(shallow, deep, depth_factor);
    float wave = sin(vWorldPos.x * 0.05 + uTime * 0.8) * 0.02 + sin(vWorldPos.z * 0.08 + uTime) * 0.02;
    color += wave * 0.3;
    color = mix(color, vec3(0.9, 0.95, 1.0), fresnel * 0.4);
    FragColor = vec4(color, 0.65);
}
)";

    const char* instanced_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in mat4 iModel;
layout(location = 7) in vec4 iColor;
layout(location = 8) in float iScalar;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out vec3 vWorldPos;
out vec4 vColor;
out float vScalar;

void main() {
    vec4 worldPos = iModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    mat3 normalMatrix = mat3(transpose(inverse(iModel)));
    vNormal = normalize(normalMatrix * aNormal);
    vColor = iColor;
    vScalar = iScalar;
    gl_Position = uProjection * uView * worldPos;
}
)";

    const char* instanced_fs = R"(#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec4 vColor;
in float vScalar;

uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform bool uOverlayScalar;
uniform float uScalarMix;

out vec4 FragColor;

vec3 heatmap(float t) {
    t = clamp(t, 0.0, 1.0);
    vec3 c1 = vec3(0.1, 0.35, 0.95);
    vec3 c2 = vec3(0.2, 0.85, 0.45);
    vec3 c3 = vec3(0.95, 0.30, 0.18);
    return (t < 0.5) ? mix(c1, c2, t * 2.0) : mix(c2, c3, (t - 0.5) * 2.0);
}

void main() {
    vec3 normal = normalize(vNormal);
    float diff = max(dot(normal, normalize(uLightDir)), 0.0);
    vec3 base = vColor.rgb;
    if (uOverlayScalar) {
        vec3 overlay = heatmap(vScalar);
        base = mix(base, overlay, clamp(uScalarMix, 0.0, 1.0));
    }
    vec3 lighting = base * (0.25 + 0.75 * diff);
    FragColor = vec4(lighting, vColor.a);
}
)";

    GLuint terrain_program = 0;
    GLuint water_program = 0;
    GLuint instanced_program = 0;
    try {
        terrain_program = link_program(compile_shader(GL_VERTEX_SHADER, terrain_vs),
                                       compile_shader(GL_FRAGMENT_SHADER, terrain_fs));
        water_program =
            link_program(compile_shader(GL_VERTEX_SHADER, water_vs), compile_shader(GL_FRAGMENT_SHADER, water_fs));
        instanced_program = link_program(compile_shader(GL_VERTEX_SHADER, instanced_vs),
                                         compile_shader(GL_FRAGMENT_SHADER, instanced_fs));
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "Shader error: %s\n", ex.what());
        if (terrain_program != 0U) glDeleteProgram(terrain_program);
        if (water_program != 0U) glDeleteProgram(water_program);
        if (instanced_program != 0U) glDeleteProgram(instanced_program);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    evo::SimulationApp app;
    genetics::GenomeStorage genome_storage;

    evo::SimulationScenario scenario{};
    scenario.environment.terrain.width_cells = 320;
    scenario.environment.terrain.height_cells = 320;
    scenario.environment.terrain.cell_size = 1.5;
    scenario.environment.terrain.elevation_scale = 32.0;
    scenario.environment.terrain.seed = 42;

    scenario.environment.soil.width_cells = 160;
    scenario.environment.soil.height_cells = 160;
    scenario.environment.soil.cell_size = 3.0;

    scenario.environment.biome.biome_count = 4;
    scenario.environment.water.water_level_percentile = 0.22;
    scenario.environment.plants.initial_count = 800;
    scenario.environment.plants.seed = 2025;
    scenario.initial_population = 36;
    scenario.genome_seed = 1337;
    scenario.reproduction_seed = 0xBEEF;

    evo::setup_scenario(app, genome_storage, scenario);
    evo::update_environment_stats(app.registry());

    auto& registry = app.registry();
    const auto* terrain_ptr = registry.ctx().find<evo::Terrain>();
    if (terrain_ptr == nullptr) {
        std::fprintf(stderr, "Terrain not found in registry context\n");
        return EXIT_FAILURE;
    }
    const auto* biome_map = registry.ctx().find<evo::BiomeMap>();
    const auto* water_map = registry.ctx().find<evo::WaterMap>();

    const evo::Terrain& terrain = *terrain_ptr;
    Mesh terrain_mesh = create_terrain_mesh_with_biomes(terrain, biome_map);

    // Initialize terrain textures
    client::TerrainTextures terrain_textures{512};
    Mesh water_mesh{};
    double water_level = 0.0;
    if (water_map != nullptr) {
        const double width = static_cast<double>(terrain.width()) * terrain.cell_size();
        const double height = static_cast<double>(terrain.height_cells()) * terrain.cell_size();
        water_level = water_map->water_level();
        water_mesh = create_water_mesh(water_level, width, height);
    }

    InstancedGeometry agents{};
    agents.mesh = create_uv_sphere_mesh();
    agents.instances = create_instance_buffer(agents.mesh.vao);

    InstancedGeometry plants{};
    plants.mesh = create_cylinder_mesh();
    plants.instances = create_instance_buffer(plants.mesh.vao);

    OrbitCamera camera{};
    camera.target = glm::vec3(static_cast<float>(terrain.width() * terrain.cell_size() * 0.5),
                              static_cast<float>((terrain.min_y() + terrain.max_y()) * 0.5),
                              static_cast<float>(terrain.height_cells() * terrain.cell_size() * 0.5));
    camera.distance = std::max(terrain.width(), terrain.height_cells()) * terrain.cell_size() * 1.4F;

    TimeControls time_controls{};
    RenderToggles toggles{};
    KeyLatch key_latch{};
    TransformHistory history{};
    history.initialize(registry);

    std::vector<InstanceData> agent_instances;
    agent_instances.reserve(1024);
    std::vector<InstanceData> plant_instances;
    plant_instances.reserve(2048);

    const glm::vec3 kLightDirection = glm::normalize(glm::vec3(0.35F, 1.0F, 0.3F));
    const double fixed_dt = app.fixed_dt();
    double accumulator = 0.0;
    double last_time = glfwGetTime();

    FrameTimings timings{};

    while (!glfwWindowShouldClose(window)) {
        const double current_time = glfwGetTime();
        double real_dt = current_time - last_time;
        last_time = current_time;

        glfwPollEvents();
        update_camera(window, camera, static_cast<float>(real_dt));
        handle_hotkeys(window, time_controls, toggles, key_latch);

        if (time_controls.paused) {
            if (time_controls.single_step) {
                accumulator += fixed_dt;
                time_controls.single_step = false;
            }
        } else {
            accumulator += real_dt * time_controls.time_scale;
        }
        accumulator = std::min(accumulator, 0.25);

        std::size_t ticks_this_frame = 0;
        const auto sim_start = std::chrono::steady_clock::now();
        while (accumulator >= fixed_dt) {
            app.tick();
            history.capture(registry);
            accumulator -= fixed_dt;
            ++ticks_this_frame;
        }
        const auto sim_end = std::chrono::steady_clock::now();

        evo::update_environment_stats(registry);
        const auto* env_stats = registry.ctx().find<evo::EnvironmentStats>();

        const double alpha = accumulator / fixed_dt;
        const GatherResult agent_stats =
            build_agent_instances(registry, history, alpha, agent_instances);
        const GatherResult plant_stats =
            build_plant_instances(registry, history, alpha, plant_instances);

        const auto render_start = std::chrono::steady_clock::now();

        int fb_width = 0;
        int fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        glClearColor(0.08F, 0.09F, 0.12F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::vec3 eye = compute_camera_position(camera);
        const glm::mat4 view = glm::lookAt(eye, camera.target, glm::vec3(0.0F, 1.0F, 0.0F));
        const float aspect =
            fb_height > 0 ? static_cast<float>(fb_width) / static_cast<float>(fb_height) : 1.0F;
        const glm::mat4 projection =
            glm::perspective(glm::radians(45.0F), aspect, 0.5F, 4000.0F);

        glUseProgram(terrain_program);
        glUniformMatrix4fv(glGetUniformLocation(terrain_program, "uModel"), 1, GL_FALSE,
                           glm::value_ptr(glm::mat4(1.0F)));
        glUniformMatrix4fv(glGetUniformLocation(terrain_program, "uView"), 1, GL_FALSE,
                           glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(terrain_program, "uProjection"), 1, GL_FALSE,
                           glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(terrain_program, "uFallbackColor"), 1,
                     glm::value_ptr(glm::vec3(0.45F, 0.75F, 0.35F)));
        glUniform1i(glGetUniformLocation(terrain_program, "uUseVertexColor"),
                    toggles.show_biome_overlay ? 1 : 0);
        glUniform1i(glGetUniformLocation(terrain_program, "uEnableTriplanar"),
                    toggles.enable_triplanar ? 1 : 0);
        glUniform1i(glGetUniformLocation(terrain_program, "uEnableNormalMaps"),
                    toggles.enable_normal_maps ? 1 : 0);
        glUniform1i(glGetUniformLocation(terrain_program, "uEnableStochastic"),
                    toggles.enable_stochastic ? 1 : 0);
        glUniform1f(glGetUniformLocation(terrain_program, "uCliffThreshold"),
                    toggles.cliff_threshold);
        glUniform1f(glGetUniformLocation(terrain_program, "uStochasticIntensity"),
                    toggles.stochastic_intensity);
        glUniform3fv(glGetUniformLocation(terrain_program, "uLightDir"), 1,
                     glm::value_ptr(kLightDirection));
        glUniform3fv(glGetUniformLocation(terrain_program, "uViewPos"), 1, glm::value_ptr(eye));
        glUniform1f(glGetUniformLocation(terrain_program, "uTerrainMinY"),
                    static_cast<float>(terrain.min_y()));
        glUniform1f(glGetUniformLocation(terrain_program, "uTerrainMaxY"),
                    static_cast<float>(terrain.max_y()));
        glUniform1f(glGetUniformLocation(terrain_program, "uWaterLevel"),
                    static_cast<float>(water_level));
        glUniform1f(glGetUniformLocation(terrain_program, "uGlobalSeed"),
                    static_cast<float>(scenario.environment.terrain.seed));
        glUniform1i(glGetUniformLocation(terrain_program, "uAlbedoArray"), 0);
        glUniform1i(glGetUniformLocation(terrain_program, "uNormalArray"), 1);
        terrain_textures.bind_albedo_array(0);
        terrain_textures.bind_normal_array(1);
        glBindVertexArray(terrain_mesh.vao);
        glDrawElements(GL_TRIANGLES, terrain_mesh.index_count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (toggles.show_water && water_mesh.vao != 0U) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glUseProgram(water_program);
            glUniformMatrix4fv(glGetUniformLocation(water_program, "uModel"), 1, GL_FALSE,
                               glm::value_ptr(glm::mat4(1.0F)));
            glUniformMatrix4fv(glGetUniformLocation(water_program, "uView"), 1, GL_FALSE,
                               glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(water_program, "uProjection"), 1, GL_FALSE,
                               glm::value_ptr(projection));
            glUniform3fv(glGetUniformLocation(water_program, "uViewPos"), 1, glm::value_ptr(eye));
            glUniform1f(glGetUniformLocation(water_program, "uWaterLevel"),
                        static_cast<float>(water_level));
            glUniform1f(glGetUniformLocation(water_program, "uTime"),
                        static_cast<float>(glfwGetTime()));
            glBindVertexArray(water_mesh.vao);
            glDrawElements(GL_TRIANGLES, water_mesh.index_count, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        glUseProgram(instanced_program);
        glUniformMatrix4fv(glGetUniformLocation(instanced_program, "uView"), 1, GL_FALSE,
                           glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(instanced_program, "uProjection"), 1, GL_FALSE,
                           glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(instanced_program, "uLightDir"), 1,
                     glm::value_ptr(kLightDirection));
        glUniform3fv(glGetUniformLocation(instanced_program, "uViewPos"), 1, glm::value_ptr(eye));
        glUniform1i(glGetUniformLocation(instanced_program, "uOverlayScalar"),
                    toggles.show_energy_overlay ? 1 : 0);
        glUniform1f(glGetUniformLocation(instanced_program, "uScalarMix"),
                    toggles.energy_overlay_strength);

        if (toggles.show_agents && !agent_instances.empty()) {
            upload_instance_data(agents.instances, agent_instances);
            glBindVertexArray(agents.mesh.vao);
            glDrawElementsInstanced(GL_TRIANGLES, agents.mesh.index_count, GL_UNSIGNED_INT, nullptr,
                                    static_cast<GLsizei>(agent_instances.size()));
            glBindVertexArray(0);
        }

        if (toggles.show_plants && !plant_instances.empty()) {
            upload_instance_data(plants.instances, plant_instances);
            glBindVertexArray(plants.mesh.vao);
            glDrawElementsInstanced(GL_TRIANGLES, plants.mesh.index_count, GL_UNSIGNED_INT,
                                    nullptr, static_cast<GLsizei>(plant_instances.size()));
            glBindVertexArray(0);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw_simulation_hud(time_controls, toggles, timings, app.simulation_time(), agent_stats,
                            plant_stats, env_stats);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        const auto render_end = std::chrono::steady_clock::now();

        timings.ticks = ticks_this_frame;
        timings.sim_ms =
            std::chrono::duration<double, std::milli>(sim_end - sim_start).count();
        timings.render_ms =
            std::chrono::duration<double, std::milli>(render_end - render_start).count();
        timings.frame_ms =
            std::chrono::duration<double, std::milli>(render_end - render_start).count();
    }

    destroy_mesh(terrain_mesh);
    destroy_mesh(water_mesh);
    destroy_instance_buffer(agents.instances);
    destroy_mesh(agents.mesh);
    destroy_instance_buffer(plants.instances);
    destroy_mesh(plants.mesh);
    glDeleteProgram(terrain_program);
    glDeleteProgram(water_program);
    glDeleteProgram(instanced_program);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}

