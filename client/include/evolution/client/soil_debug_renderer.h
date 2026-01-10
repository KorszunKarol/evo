#pragma once

#include <cstdint>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "evolution/sim/environment/soil_volume.h"

namespace evolution::client {

struct SoilVoxelInstance {
    glm::mat4 model;
    glm::vec4 color;
};

class SoilDebugRenderer {
public:
    SoilDebugRenderer();
    ~SoilDebugRenderer();

    void build_mesh();
    void upload_instances(const std::vector<SoilVoxelInstance>& instances);
    void render(const glm::mat4& view, const glm::mat4& projection);

private:
    GLuint vao_{0};
    GLuint vbo_{0}; // Cube vertices
    GLuint ebo_{0}; // Cube indices
    GLuint instance_vbo_{0}; // Instance data
    GLuint program_{0};
    GLsizei index_count_{0};
    std::size_t instance_capacity_{0};
    std::size_t instance_count_{0};
};

void extract_soil_voxels(const sim::SoilVolume& volume, 
                         std::vector<SoilVoxelInstance>& out_instances,
                         float threshold = 0.1f);

} // namespace evolution::client






