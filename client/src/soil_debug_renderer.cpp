#include "evolution/client/soil_debug_renderer.h"

#include <cstdio>
#include <stdexcept>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace evolution::client {

namespace {

const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in mat4 iModel;
layout(location = 5) in vec4 iColor;

uniform mat4 uView;
uniform mat4 uProjection;

out vec4 vColor;

void main() {
    gl_Position = uProjection * uView * iModel * vec4(aPos, 1.0);
    vColor = iColor;
}
)";

const char* kFragmentShader = R"(#version 330 core
in vec4 vColor;
out vec4 FragColor;

void main() {
    FragColor = vColor;
}
)";

GLuint compile_shader(GLenum type, const char* source) {
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

GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
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

} // namespace

SoilDebugRenderer::SoilDebugRenderer() {
    try {
        program_ = link_program(compile_shader(GL_VERTEX_SHADER, kVertexShader),
                                compile_shader(GL_FRAGMENT_SHADER, kFragmentShader));
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "SoilDebugRenderer Shader error: %s\n", ex.what());
    }
    build_mesh();
}

SoilDebugRenderer::~SoilDebugRenderer() {
    if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
    if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
    if (ebo_ != 0) glDeleteBuffers(1, &ebo_);
    if (instance_vbo_ != 0) glDeleteBuffers(1, &instance_vbo_);
    if (program_ != 0) glDeleteProgram(program_);
}

void SoilDebugRenderer::build_mesh() {
    // Simple cube
    const float vertices[] = {
        // Front face
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        // Back face
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
    };

    const std::uint32_t indices[] = {
        // Front
        0, 1, 2, 2, 3, 0,
        // Back
        5, 4, 7, 7, 6, 5,
        // Left
        4, 0, 3, 3, 7, 4,
        // Right
        1, 5, 6, 6, 2, 1,
        // Top
        3, 2, 6, 6, 7, 3,
        // Bottom
        4, 5, 1, 1, 0, 4
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glGenBuffers(1, &instance_vbo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Instance attributes
    glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
    
    // Mat4 (takes 4 vec4 slots)
    std::size_t stride = sizeof(SoilVoxelInstance);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(glm::vec4) * i));
        glVertexAttribDivisor(1 + i, 1);
    }

    // Color
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)(offsetof(SoilVoxelInstance, color)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
    index_count_ = 36;
}

void SoilDebugRenderer::upload_instances(const std::vector<SoilVoxelInstance>& instances) {
    if (instances.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
    if (instances.size() > instance_capacity_) {
        instance_capacity_ = instances.size() * 2;
        glBufferData(GL_ARRAY_BUFFER, instance_capacity_ * sizeof(SoilVoxelInstance), 
                     instances.data(), GL_DYNAMIC_DRAW);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, instances.size() * sizeof(SoilVoxelInstance), 
                        instances.data());
    }
    instance_count_ = instances.size();
}

void SoilDebugRenderer::render(const glm::mat4& view, const glm::mat4& projection) {
    if (instance_count_ == 0) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Optional: Disable depth write for transparent objects to see through them
    glDepthMask(GL_FALSE); 

    glUseProgram(program_);
    glUniformMatrix4fv(glGetUniformLocation(program_, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program_, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(vao_);
    glDrawElementsInstanced(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(instance_count_));
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void extract_soil_voxels(const sim::SoilVolume& volume, 
                         std::vector<SoilVoxelInstance>& out_instances,
                         float threshold) {
    out_instances.clear();
    
    const int w = volume.width();
    const int h = volume.height();
    const int d = volume.depth();
    const double voxel_size = volume.voxel_size();

    // Optimization: Reserve based on a guess (e.g. 10% filled)
    out_instances.reserve(w * h * d / 10);

    for (int z = 0; z < d; ++z) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const auto& voxel = volume.at(x, y, z);
                double n = voxel.nitrogen.to_double();
                double water = voxel.water.to_double();

                // Only draw if there is something interesting
                if (n < threshold && water < threshold) continue;

                SoilVoxelInstance inst;
                
                // Calculate world position (center of voxel)
                double wx = (x + 0.5) * voxel_size;
                double wz = (z + 0.5) * voxel_size;
                // Assuming Y goes down? Or Up? Soil usually implies depth.
                // Let's map Y index 0 to terrain surface (0) and go down?
                // Or simply stack them up. Let's stack them up for now to see them.
                // If soil is "underground", we might need negative Y. 
                // For visualization, let's put them slightly *above* ground to see them clearly,
                // or allow them to overlap terrain.
                double wy = (y + 0.5) * voxel_size; 

                inst.model = glm::translate(glm::mat4(1.0f), glm::vec3(wx, wy, wz));
                inst.model = glm::scale(inst.model, glm::vec3(voxel_size * 0.9f)); // Shrink slightly to see gaps

                // Color mapping: Blue = Water, Green = Nitrogen
                // Mix them based on content
                glm::vec3 color(0.0f);
                float alpha = 0.0f;

                if (water > n) {
                    color = glm::vec3(0.2f, 0.4f, 0.9f); // Blueish
                    alpha = std::min(1.0f, static_cast<float>(water) * 0.5f);
                } else {
                    color = glm::vec3(0.2f, 0.8f, 0.2f); // Greenish
                    alpha = std::min(1.0f, static_cast<float>(n) * 0.5f);
                }
                
                // Minimum visibility
                alpha = std::max(alpha, 0.2f);

                inst.color = glm::vec4(color, alpha * 0.6f); // Global transparency
                out_instances.push_back(inst);
            }
        }
    }
}

} // namespace evolution::client






