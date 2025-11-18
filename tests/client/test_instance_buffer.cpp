#include "test_render_fixtures.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace evolution::client::test {

// Instance buffer structures matching main.cpp
namespace {
struct InstanceBuffer {
    GLuint buffer{0};
    std::size_t capacity{0};
};

struct InstanceData {
    glm::mat4 model{1.0F};
    glm::vec4 color{1.0F};
    float debug_scalar{0.0F};
    float padding[3]{};
};

void destroy_instance_buffer(InstanceBuffer& buffer) {
    if (buffer.buffer != 0U) {
        glDeleteBuffers(1, &buffer.buffer);
        buffer.buffer = 0;
    }
    buffer.capacity = 0;
}

InstanceBuffer create_instance_buffer(GLuint vao) {
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
    const std::size_t new_capacity =
        std::max(buffer.capacity * 2, std::max<std::size_t>(64, count));
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
}  // namespace

TEST_F(RenderClientFixture, InstanceBufferCreation) {
    gl_context().make_current();
    gl_context().clear_errors();

    // Create a dummy VAO for the instance buffer
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    InstanceBuffer buffer = create_instance_buffer(vao);

    EXPECT_NE(buffer.buffer, 0U);
    EXPECT_EQ(buffer.capacity, 0U);

    gl_context().check_gl_errors("instance buffer creation");

    destroy_instance_buffer(buffer);
    glDeleteVertexArrays(1, &vao);
}

TEST_F(RenderClientFixture, InstanceBufferCapacityExpansion) {
    gl_context().make_current();
    gl_context().clear_errors();

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    InstanceBuffer buffer = create_instance_buffer(vao);

    // Initially empty
    EXPECT_EQ(buffer.capacity, 0U);

    // Request capacity for 10 instances
    ensure_instance_capacity(buffer, 10);
    EXPECT_GE(buffer.capacity, 10U);

    // Request capacity for 100 instances
    ensure_instance_capacity(buffer, 100);
    EXPECT_GE(buffer.capacity, 100U);

    // Request capacity for 50 instances (should not shrink)
    const std::size_t previous_capacity = buffer.capacity;
    ensure_instance_capacity(buffer, 50);
    EXPECT_EQ(buffer.capacity, previous_capacity);  // Should not shrink

    gl_context().check_gl_errors("instance buffer capacity");

    destroy_instance_buffer(buffer);
    glDeleteVertexArrays(1, &vao);
}

TEST_F(RenderClientFixture, InstanceBufferUpload) {
    gl_context().make_current();
    gl_context().clear_errors();

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    InstanceBuffer buffer = create_instance_buffer(vao);

    // Create test instance data
    std::vector<InstanceData> instances;
    for (int i = 0; i < 5; ++i) {
        InstanceData instance{};
        instance.model = glm::translate(glm::mat4(1.0F), glm::vec3(static_cast<float>(i), 0.0F, 0.0F));
        instance.color = glm::vec4(1.0F, 0.0F, 0.0F, 1.0F);
        instance.debug_scalar = static_cast<float>(i) / 10.0F;
        instances.push_back(instance);
    }

    upload_instance_data(buffer, instances);

    EXPECT_GE(buffer.capacity, instances.size());

    // Verify buffer was uploaded
    glBindBuffer(GL_ARRAY_BUFFER, buffer.buffer);
    GLint buffer_size = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &buffer_size);
    EXPECT_GE(buffer_size, static_cast<GLint>(instances.size() * sizeof(InstanceData)));

    gl_context().check_gl_errors("instance buffer upload");

    destroy_instance_buffer(buffer);
    glDeleteVertexArrays(1, &vao);
}

TEST_F(RenderClientFixture, InstanceBufferUploadEmpty) {
    gl_context().make_current();
    gl_context().clear_errors();

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    InstanceBuffer buffer = create_instance_buffer(vao);

    std::vector<InstanceData> empty_instances;
    upload_instance_data(buffer, empty_instances);

    // Should not crash or error
    EXPECT_EQ(buffer.capacity, 0U);

    gl_context().check_gl_errors("instance buffer upload empty");

    destroy_instance_buffer(buffer);
    glDeleteVertexArrays(1, &vao);
}

TEST_F(RenderClientFixture, InstanceBufferMultipleUploads) {
    gl_context().make_current();
    gl_context().clear_errors();

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    InstanceBuffer buffer = create_instance_buffer(vao);

    // First upload: 10 instances
    std::vector<InstanceData> instances1;
    instances1.resize(10);
    upload_instance_data(buffer, instances1);
    const std::size_t capacity1 = buffer.capacity;

    // Second upload: 5 instances (should reuse capacity)
    std::vector<InstanceData> instances2;
    instances2.resize(5);
    upload_instance_data(buffer, instances2);
    EXPECT_EQ(buffer.capacity, capacity1);  // Should not expand

    // Third upload: 100 instances (should expand)
    std::vector<InstanceData> instances3;
    instances3.resize(100);
    upload_instance_data(buffer, instances3);
    EXPECT_GE(buffer.capacity, 100U);

    gl_context().check_gl_errors("instance buffer multiple uploads");

    destroy_instance_buffer(buffer);
    glDeleteVertexArrays(1, &vao);
}

TEST_F(RenderClientFixture, InstanceBufferCleanup) {
    gl_context().make_current();
    gl_context().clear_errors();

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    InstanceBuffer buffer = create_instance_buffer(vao);
    ensure_instance_capacity(buffer, 100);
    EXPECT_NE(buffer.buffer, 0U);
    EXPECT_GT(buffer.capacity, 0U);

    destroy_instance_buffer(buffer);

    EXPECT_EQ(buffer.buffer, 0U);
    EXPECT_EQ(buffer.capacity, 0U);

    gl_context().check_gl_errors("instance buffer cleanup");

    glDeleteVertexArrays(1, &vao);
}

}  // namespace evolution::client::test

