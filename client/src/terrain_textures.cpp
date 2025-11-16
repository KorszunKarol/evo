#include "evolution/client/terrain_textures.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>

namespace evolution::client {

namespace {

/**
 * @brief Simple hash function for deterministic noise.
 *
 * @param x Input value
 * @return float Pseudo-random value in [0, 1)
 * @complexity O(1)
 */
[[nodiscard]] float hash(float x) noexcept {
    return std::fmod(std::sin(x * 12.9898F) * 43758.5453F, 1.0F);
}

/**
 * @brief 2D hash function for procedural generation.
 *
 * @param x First coordinate
 * @param y Second coordinate
 * @return float Pseudo-random value in [0, 1)
 * @complexity O(1)
 */
[[nodiscard]] float hash2d(float x, float y) noexcept {
    return hash(x + hash(y));
}

/**
 * @brief Simple 2D noise function.
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @return float Noise value in [0, 1)
 * @complexity O(1)
 */
[[nodiscard]] float noise2d(float x, float y) noexcept {
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const float cx = x - fx;
    const float cy = y - fy;

    const float a = hash2d(fx, fy);
    const float b = hash2d(fx + 1.0F, fy);
    const float c = hash2d(fx, fy + 1.0F);
    const float d = hash2d(fx + 1.0F, fy + 1.0F);

    const float ux = cx * cx * (3.0F - 2.0F * cx);
    const float uy = cy * cy * (3.0F - 2.0F * cy);

    const float ab = a + (b - a) * ux;
    const float cd = c + (d - c) * ux;
    return ab + (cd - ab) * uy;
}

/**
 * @brief Fractal noise with multiple octaves.
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @param octaves Number of octaves
 * @return float Noise value in [0, 1)
 * @complexity O(octaves)
 */
[[nodiscard]] float fbm(float x, float y, int octaves = 4) noexcept {
    float value = 0.0F;
    float amplitude = 0.5F;
    float frequency = 1.0F;
    float max_value = 0.0F;

    for (int i = 0; i < octaves; ++i) {
        value += amplitude * noise2d(x * frequency, y * frequency);
        max_value += amplitude;
        amplitude *= 0.5F;
        frequency *= 2.0F;
    }

    return value / max_value;
}

/**
 * @brief Converts float color component to uint8_t.
 *
 * @param f Value in [0, 1]
 * @return std::uint8_t Clamped to [0, 255]
 */
[[nodiscard]] std::uint8_t float_to_byte(float f) noexcept {
    return static_cast<std::uint8_t>(std::clamp(f * 255.0F, 0.0F, 255.0F));
}

/**
 * @brief Converts normal vector to RGB8 encoding.
 *
 * @param nx Normal X component [-1, 1]
 * @param ny Normal Y component [-1, 1]
 * @param nz Normal Z component [-1, 1]
 * @return std::array<std::uint8_t, 3> RGB values in [0, 255]
 */
[[nodiscard]] std::array<std::uint8_t, 3> normal_to_rgb(float nx, float ny, float nz) noexcept {
    return {float_to_byte((nx + 1.0F) * 0.5F), float_to_byte((ny + 1.0F) * 0.5F),
            float_to_byte((nz + 1.0F) * 0.5F)};
}

}  // namespace

TerrainTextures::TerrainTextures(int resolution)
    : resolution_{resolution} {
    constexpr int material_count = static_cast<int>(MaterialType::Count);

    // Generate albedo textures
    std::vector<std::uint8_t> albedo_data;
    albedo_data.reserve(static_cast<std::size_t>(resolution * resolution * 4 * material_count));

    for (int mat = 0; mat < material_count; ++mat) {
        const auto material = static_cast<MaterialType>(mat);
        const auto tex_data = generate_albedo(material, resolution);
        albedo_data.insert(albedo_data.end(), tex_data.begin(), tex_data.end());
    }

    // Generate normal maps
    std::vector<std::uint8_t> normal_data;
    normal_data.reserve(static_cast<std::size_t>(resolution * resolution * 4 * material_count));

    for (int mat = 0; mat < material_count; ++mat) {
        const auto material = static_cast<MaterialType>(mat);
        const auto tex_data = generate_normal(material, resolution);
        normal_data.insert(normal_data.end(), tex_data.begin(), tex_data.end());
    }

    // Create texture arrays
    glGenTextures(1, &albedo_array_);
    glGenTextures(1, &normal_array_);

    if (albedo_array_ == 0U || normal_array_ == 0U) {
        if (albedo_array_ != 0U) {
            glDeleteTextures(1, &albedo_array_);
        }
        if (normal_array_ != 0U) {
            glDeleteTextures(1, &normal_array_);
        }
        throw std::runtime_error("Failed to create terrain texture arrays");
    }

    // Upload albedo array
    glBindTexture(GL_TEXTURE_2D_ARRAY, albedo_array_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, resolution, resolution, material_count, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, albedo_data.data());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    // Upload normal array
    glBindTexture(GL_TEXTURE_2D_ARRAY, normal_array_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, resolution, resolution, material_count, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, normal_data.data());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

TerrainTextures::~TerrainTextures() {
    if (albedo_array_ != 0U) {
        glDeleteTextures(1, &albedo_array_);
    }
    if (normal_array_ != 0U) {
        glDeleteTextures(1, &normal_array_);
    }
}

TerrainTextures::TerrainTextures(TerrainTextures&& other) noexcept
    : albedo_array_{other.albedo_array_}
    , normal_array_{other.normal_array_}
    , resolution_{other.resolution_} {
    other.albedo_array_ = 0;
    other.normal_array_ = 0;
    other.resolution_ = 0;
}

TerrainTextures& TerrainTextures::operator=(TerrainTextures&& other) noexcept {
    if (this != &other) {
        if (albedo_array_ != 0U) {
            glDeleteTextures(1, &albedo_array_);
        }
        if (normal_array_ != 0U) {
            glDeleteTextures(1, &normal_array_);
        }

        albedo_array_ = other.albedo_array_;
        normal_array_ = other.normal_array_;
        resolution_ = other.resolution_;

        other.albedo_array_ = 0;
        other.normal_array_ = 0;
        other.resolution_ = 0;
    }
    return *this;
}

void TerrainTextures::bind_albedo_array(GLuint unit) const noexcept {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, albedo_array_);
}

void TerrainTextures::bind_normal_array(GLuint unit) const noexcept {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, normal_array_);
}

std::vector<std::uint8_t> TerrainTextures::generate_albedo(MaterialType material,
                                                            int resolution) noexcept {
    std::vector<std::uint8_t> data;
    data.reserve(static_cast<std::size_t>(resolution * resolution * 4));

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(resolution);
            const float fy = static_cast<float>(y) / static_cast<float>(resolution);

            float r = 0.5F;
            float g = 0.5F;
            float b = 0.5F;

            switch (material) {
                case MaterialType::Grass: {
                    const float n = fbm(fx * 8.0F, fy * 8.0F, 3);
                    r = 0.3F + n * 0.2F;
                    g = 0.6F + n * 0.3F;
                    b = 0.2F + n * 0.15F;
                    break;
                }
                case MaterialType::Soil: {
                    const float n = fbm(fx * 6.0F, fy * 6.0F, 3);
                    r = 0.4F + n * 0.25F;
                    g = 0.35F + n * 0.2F;
                    b = 0.25F + n * 0.15F;
                    break;
                }
                case MaterialType::Rock: {
                    const float n = fbm(fx * 4.0F, fy * 4.0F, 4);
                    r = 0.45F + n * 0.2F;
                    g = 0.45F + n * 0.2F;
                    b = 0.45F + n * 0.2F;
                    break;
                }
                case MaterialType::Sand: {
                    const float n = fbm(fx * 10.0F, fy * 10.0F, 2);
                    r = 0.85F + n * 0.1F;
                    g = 0.75F + n * 0.1F;
                    b = 0.55F + n * 0.1F;
                    break;
                }
                case MaterialType::Snow: {
                    const float n = fbm(fx * 12.0F, fy * 12.0F, 2);
                    r = 0.9F + n * 0.08F;
                    g = 0.9F + n * 0.08F;
                    b = 0.95F + n * 0.05F;
                    break;
                }
                case MaterialType::Count:
                    break;
            }

            data.push_back(float_to_byte(r));
            data.push_back(float_to_byte(g));
            data.push_back(float_to_byte(b));
            data.push_back(255);  // Alpha
        }
    }

    return data;
}

std::vector<std::uint8_t> TerrainTextures::generate_normal(MaterialType material,
                                                             int resolution) noexcept {
    std::vector<std::uint8_t> data;
    data.reserve(static_cast<std::size_t>(resolution * resolution * 4));

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(resolution);
            const float fy = static_cast<float>(y) / static_cast<float>(resolution);

            float nx = 0.0F;
            float ny = 1.0F;
            float nz = 0.0F;

            switch (material) {
                case MaterialType::Grass: {
                    const float scale = 0.15F;
                    nx = (fbm(fx * 16.0F, fy * 16.0F, 2) - 0.5F) * scale;
                    nz = (fbm(fx * 16.0F + 100.0F, fy * 16.0F + 100.0F, 2) - 0.5F) * scale;
                    ny = std::sqrt(1.0F - nx * nx - nz * nz);
                    break;
                }
                case MaterialType::Soil: {
                    const float scale = 0.1F;
                    nx = (fbm(fx * 12.0F, fy * 12.0F, 2) - 0.5F) * scale;
                    nz = (fbm(fx * 12.0F + 50.0F, fy * 12.0F + 50.0F, 2) - 0.5F) * scale;
                    ny = std::sqrt(1.0F - nx * nx - nz * nz);
                    break;
                }
                case MaterialType::Rock: {
                    const float scale = 0.25F;
                    nx = (fbm(fx * 8.0F, fy * 8.0F, 3) - 0.5F) * scale;
                    nz = (fbm(fx * 8.0F + 200.0F, fy * 8.0F + 200.0F, 3) - 0.5F) * scale;
                    ny = std::sqrt(1.0F - nx * nx - nz * nz);
                    break;
                }
                case MaterialType::Sand: {
                    const float scale = 0.08F;
                    nx = (fbm(fx * 20.0F, fy * 20.0F, 2) - 0.5F) * scale;
                    nz = (fbm(fx * 20.0F + 300.0F, fy * 20.0F + 300.0F, 2) - 0.5F) * scale;
                    ny = std::sqrt(1.0F - nx * nx - nz * nz);
                    break;
                }
                case MaterialType::Snow: {
                    const float scale = 0.05F;
                    nx = (fbm(fx * 24.0F, fy * 24.0F, 2) - 0.5F) * scale;
                    nz = (fbm(fx * 24.0F + 400.0F, fy * 24.0F + 400.0F, 2) - 0.5F) * scale;
                    ny = std::sqrt(1.0F - nx * nx - nz * nz);
                    break;
                }
                case MaterialType::Count:
                    break;
            }

            const auto rgb = normal_to_rgb(nx, ny, nz);
            data.push_back(rgb[0]);
            data.push_back(rgb[1]);
            data.push_back(rgb[2]);
            data.push_back(255);  // Alpha
        }
    }

    return data;
}

}  // namespace evolution::client

