#include "test_render_fixtures.h"

#include <glad/glad.h>

#include "evolution/client/terrain_textures.h"

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

TEST_F(RenderClientFixture, TerrainTexturesCreation) {
    gl_context().make_current();
    gl_context().clear_errors();

    EXPECT_NO_THROW({
        TerrainTextures textures{256};
        EXPECT_NE(textures.albedo_array(), 0U);
        EXPECT_NE(textures.normal_array(), 0U);
        EXPECT_EQ(textures.material_count(), 5);
    });

    gl_context().check_gl_errors("terrain textures creation");
}

TEST_F(RenderClientFixture, TerrainTexturesBinding) {
    gl_context().make_current();
    gl_context().clear_errors();

    TerrainTextures textures{128};
    textures.bind_albedo_array(0);
    textures.bind_normal_array(1);

    gl_context().check_gl_errors("terrain textures binding");
}

TEST_F(RenderClientFixture, AdvancedTerrainShaderCompiles) {
    gl_context().make_current();
    gl_context().clear_errors();

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

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

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

vec4 sampleTriplanar(sampler2DArray tex, vec3 worldPos, vec3 normal, int materialIndex, float scale) {
    vec3 absNormal = abs(normal);
    float totalWeight = absNormal.x + absNormal.y + absNormal.z;
    absNormal /= totalWeight;
    vec4 sampleX = texture(tex, vec3(worldPos.zy * scale, float(materialIndex)));
    vec4 sampleY = texture(tex, vec3(worldPos.xz * scale, float(materialIndex)));
    vec4 sampleZ = texture(tex, vec3(worldPos.xy * scale, float(materialIndex)));
    return sampleX * absNormal.x + sampleY * absNormal.y + sampleZ * absNormal.z;
}

float macroNoise(vec2 p) {
    return fract(sin(dot(p + vec2(uGlobalSeed), vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec3 worldPos = vWorldPos;
    vec3 geomNormal = normalize(vNormal);
    float steepness = 1.0 - dot(geomNormal, vec3(0.0, 1.0, 0.0));
    float useTriplanar = uEnableTriplanar ? smoothstep(uCliffThreshold - 0.1, uCliffThreshold + 0.1, steepness) : 0.0;
    int materialIndex = 0;
    float heightNormalized = (worldPos.y - uTerrainMinY) / max(uTerrainMaxY - uTerrainMinY, 0.001);
    if (heightNormalized > 0.7) {
        materialIndex = 4;
    } else if (steepness > 0.5) {
        materialIndex = 2;
    } else if (worldPos.y < uWaterLevel + 2.0) {
        materialIndex = 3;
    }
    vec2 macroCoord = worldPos.xz * 0.01;
    float macroVariation = macroNoise(macroCoord) * 0.1 + 0.95;
    float texScale = 0.1;
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
    albedo.rgb *= macroVariation;
    vec3 finalNormal = geomNormal;
    if (uEnableNormalMaps) {
        vec3 texCoord = vec3(worldPos.xz * texScale, float(materialIndex));
        vec3 normalMapSample = texture(uNormalArray, texCoord).rgb * 2.0 - 1.0;
        vec3 tangent = normalize(vec3(1.0, 0.0, geomNormal.x));
        vec3 bitangent = cross(geomNormal, tangent);
        mat3 tbn = mat3(tangent, bitangent, geomNormal);
        normalMapSample = normalize(tbn * normalMapSample);
        finalNormal = normalize(mix(geomNormal, normalMapSample, 0.7));
    }
    if (uUseVertexColor) {
        albedo.rgb = vColor;
    }
    vec3 lightDir = normalize(uLightDir);
    float diff = max(dot(finalNormal, lightDir), 0.0);
    vec3 lit = albedo.rgb * (0.2 + 0.8 * diff);
    FragColor = vec4(lit, 1.0);
}
)";

    GLuint vs = compile_shader(GL_VERTEX_SHADER, terrain_vs);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, terrain_fs);
    GLuint program = link_program(vs, fs);

    EXPECT_NE(program, 0U);
    gl_context().check_gl_errors("advanced terrain shader");

    glDeleteProgram(program);
}

TEST_F(RenderClientFixture, TerrainShaderTriplanarFunction) {
    gl_context().make_current();
    gl_context().clear_errors();

    // Test that triplanar sampling function compiles
    const char* test_fs = R"(#version 330 core
uniform sampler2DArray uTex;
out vec4 FragColor;

vec4 sampleTriplanar(sampler2DArray tex, vec3 worldPos, vec3 normal, int materialIndex, float scale) {
    vec3 absNormal = abs(normal);
    float totalWeight = absNormal.x + absNormal.y + absNormal.z;
    absNormal /= totalWeight;
    vec4 sampleX = texture(tex, vec3(worldPos.zy * scale, float(materialIndex)));
    vec4 sampleY = texture(tex, vec3(worldPos.xz * scale, float(materialIndex)));
    vec4 sampleZ = texture(tex, vec3(worldPos.xy * scale, float(materialIndex)));
    return sampleX * absNormal.x + sampleY * absNormal.y + sampleZ * absNormal.z;
}

void main() {
    FragColor = sampleTriplanar(uTex, vec3(0.0), vec3(0.0, 1.0, 0.0), 0, 1.0);
}
)";

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, test_fs);
    EXPECT_NE(fs, 0U);
    gl_context().check_gl_errors("triplanar function");
    glDeleteShader(fs);
}

TEST_F(RenderClientFixture, TerrainShaderStochasticFunction) {
    gl_context().make_current();
    gl_context().clear_errors();

    // Test that stochastic sampling function compiles
    const char* test_fs = R"(#version 330 core
uniform sampler2DArray uTex;
out vec4 FragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

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

void main() {
    FragColor = sampleStochastic(uTex, vec3(0.0), 0.5);
}
)";

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, test_fs);
    EXPECT_NE(fs, 0U);
    gl_context().check_gl_errors("stochastic function");
    glDeleteShader(fs);
}

}  // namespace evolution::client::test

