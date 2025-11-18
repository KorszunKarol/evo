#include "test_render_fixtures.h"

#include <glad/glad.h>

namespace evolution::client::test {

// Helper functions matching main.cpp anonymous namespace
namespace {
GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == 0) {
        GLchar info_log[1024];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        glDeleteShader(shader);
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
        glDeleteProgram(program);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        throw std::runtime_error(std::string("Program linking failed: ") + info_log);
    }
    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}
}  // anonymous namespace

TEST_F(RenderClientFixture, ShaderCompilationSuccess) {
    gl_context().make_current();
    gl_context().clear_errors();

    const char* valid_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

    const char* valid_fs = R"(#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

    GLuint vs = 0;
    GLuint fs = 0;
    GLuint program = 0;

    EXPECT_NO_THROW(vs = compile_shader(GL_VERTEX_SHADER, valid_vs));
    EXPECT_NE(vs, 0U);
    gl_context().check_gl_errors("vertex shader compilation");

    EXPECT_NO_THROW(fs = compile_shader(GL_FRAGMENT_SHADER, valid_fs));
    EXPECT_NE(fs, 0U);
    gl_context().check_gl_errors("fragment shader compilation");

    EXPECT_NO_THROW(program = link_program(vs, fs));
    EXPECT_NE(program, 0U);
    gl_context().check_gl_errors("program linking");

    // Cleanup
    if (program != 0U) {
        glDeleteProgram(program);
    }
}

TEST_F(RenderClientFixture, ShaderCompilationFailure) {
    gl_context().make_current();
    gl_context().clear_errors();

    const char* invalid_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
void main() {
    gl_Position = undefined_function(aPos);  // Invalid
}
)";

    EXPECT_THROW(compile_shader(GL_VERTEX_SHADER, invalid_vs), std::runtime_error);
    gl_context().clear_errors();
}

TEST_F(RenderClientFixture, ShaderLinkingFailure) {
    gl_context().make_current();
    gl_context().clear_errors();

    const char* valid_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos, 1.0);
}
)";

    const char* invalid_fs = R"(#version 330 core
out vec4 FragColor;
void main() {
    FragColor = undefined_variable;  // Invalid
}
)";

    // Test that invalid fragment shader compilation fails
    EXPECT_THROW({
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, invalid_fs);
        glDeleteShader(fs);
    }, std::runtime_error);
    
    gl_context().clear_errors();
    
    // Test linking with mismatched interfaces
    // Note: Some OpenGL drivers allow missing inputs, so this may succeed
    GLuint vs = compile_shader(GL_VERTEX_SHADER, valid_vs);
    
    const char* mismatched_fs = R"(#version 330 core
out vec4 FragColor;
in vec3 vNonExistent;  // Vertex shader doesn't output this
void main() {
    FragColor = vec4(vNonExistent, 1.0);
}
)";
    
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, mismatched_fs);
    
    // Some drivers allow this, some don't - both behaviors are acceptable
    try {
        GLuint program = link_program(vs, fs);
        // Linking succeeded (driver allowed mismatch) - this is acceptable
        glDeleteProgram(program);
    } catch (const std::runtime_error&) {
        // Linking failed (driver enforced strict matching) - also acceptable
    }
    
    // Cleanup
    glDeleteShader(vs);
    glDeleteShader(fs);
    gl_context().clear_errors();
}

TEST_F(RenderClientFixture, TerrainShaderCompiles) {
    gl_context().make_current();
    gl_context().clear_errors();

    const char* terrain_vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out vec3 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(uModel)));
    vNormal = normalize(normalMatrix * aNormal);
    vColor = aColor;
    gl_Position = uProjection * uView * worldPos;
}
)";

    const char* terrain_fs = R"(#version 330 core
in vec3 vNormal;
in vec3 vColor;

uniform vec3 uFallbackColor;
uniform bool uUseVertexColor;
uniform vec3 uLightDir;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(vNormal);
    float diff = max(dot(normal, normalize(uLightDir)), 0.0);
    vec3 base = uUseVertexColor ? vColor : uFallbackColor;
    vec3 lit = base * (0.20 + 0.80 * diff);
    FragColor = vec4(lit, 1.0);
}
)";

    GLuint vs = compile_shader(GL_VERTEX_SHADER, terrain_vs);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, terrain_fs);
    GLuint program = link_program(vs, fs);

    EXPECT_NE(program, 0U);
    gl_context().check_gl_errors("terrain shader");

    glDeleteProgram(program);
}

TEST_F(RenderClientFixture, WaterShaderCompiles) {
    gl_context().make_current();
    gl_context().clear_errors();

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

    GLuint vs = compile_shader(GL_VERTEX_SHADER, water_vs);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, water_fs);
    GLuint program = link_program(vs, fs);

    EXPECT_NE(program, 0U);
    gl_context().check_gl_errors("water shader");

    glDeleteProgram(program);
}

TEST_F(RenderClientFixture, InstancedShaderCompiles) {
    gl_context().make_current();
    gl_context().clear_errors();

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

    GLuint vs = compile_shader(GL_VERTEX_SHADER, instanced_vs);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, instanced_fs);
    GLuint program = link_program(vs, fs);

    EXPECT_NE(program, 0U);
    gl_context().check_gl_errors("instanced shader");

    glDeleteProgram(program);
}

}  // namespace evolution::client::test

