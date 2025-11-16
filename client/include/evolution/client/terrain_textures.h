#pragma once

#include <cstdint>
#include <vector>

#include <glad/glad.h>

namespace evolution::client {

/**
 * @brief Manages terrain material textures for advanced shading.
 *
 * @details Provides procedural texture generation and texture array management
 *          for terrain materials (grass, soil, rock, sand, snow). Supports
 *          both procedural generation and future texture file loading.
 *
 * @threadsafe Not thread-safe; must be used from the OpenGL context thread.
 */
class TerrainTextures {
public:
    /**
     * @brief Material type identifiers for terrain surfaces.
     */
    enum class MaterialType : std::uint8_t {
        Grass = 0,  ///< Grass/vegetation material
        Soil = 1,   ///< Dirt/soil material
        Rock = 2,   ///< Rock/cliff material
        Sand = 3,   ///< Sand/shoreline material
        Snow = 4,   ///< Snow/alpine material
        Count = 5   ///< Total number of materials
    };

    /**
     * @brief Constructs the texture manager and generates procedural textures.
     *
     * @param resolution Texture resolution (default: 512x512)
     * @throws std::runtime_error If OpenGL texture creation fails
     * @complexity O(N) where N = resolution^2 * material_count
     * @note Requires active OpenGL context
     */
    explicit TerrainTextures(int resolution = 512);

    /**
     * @brief Destroys all managed textures.
     *
     * @note Safe to call even if textures were not created
     */
    ~TerrainTextures();

    // Non-copyable, movable
    TerrainTextures(const TerrainTextures&) = delete;
    TerrainTextures& operator=(const TerrainTextures&) = delete;
    TerrainTextures(TerrainTextures&&) noexcept;
    TerrainTextures& operator=(TerrainTextures&&) noexcept;

    /**
     * @brief Binds the texture array to the specified texture unit.
     *
     * @param unit Texture unit index (0-31 typically)
     * @complexity O(1)
     */
    void bind_albedo_array(GLuint unit) const noexcept;

    /**
     * @brief Binds the normal map array to the specified texture unit.
     *
     * @param unit Texture unit index
     * @complexity O(1)
     */
    void bind_normal_array(GLuint unit) const noexcept;

    /**
     * @brief Returns the OpenGL texture ID for the albedo array.
     *
     * @return GLuint Texture object name, or 0 if not initialized
     */
    [[nodiscard]] GLuint albedo_array() const noexcept { return albedo_array_; }

    /**
     * @brief Returns the OpenGL texture ID for the normal array.
     *
     * @return GLuint Texture object name, or 0 if not initialized
     */
    [[nodiscard]] GLuint normal_array() const noexcept { return normal_array_; }

    /**
     * @brief Returns the number of materials in the arrays.
     *
     * @return int Always returns static_cast<int>(MaterialType::Count)
     */
    [[nodiscard]] static constexpr int material_count() noexcept {
        return static_cast<int>(MaterialType::Count);
    }

private:
    /**
     * @brief Generates a procedural albedo texture for a material type.
     *
     * @param material Material type to generate
     * @param resolution Texture resolution
     * @return std::vector<std::uint8_t> RGBA8 data (resolution^2 * 4 bytes)
     * @complexity O(resolution^2)
     */
    [[nodiscard]] static std::vector<std::uint8_t> generate_albedo(MaterialType material,
                                                                     int resolution) noexcept;

    /**
     * @brief Generates a procedural normal map for a material type.
     *
     * @param material Material type to generate
     * @param resolution Texture resolution
     * @return std::vector<std::uint8_t> RGBA8 data (normal maps stored as RGB, A unused)
     * @complexity O(resolution^2)
     */
    [[nodiscard]] static std::vector<std::uint8_t> generate_normal(MaterialType material,
                                                                    int resolution) noexcept;

    GLuint albedo_array_{0};
    GLuint normal_array_{0};
    int resolution_{512};
};

}  // namespace evolution::client

