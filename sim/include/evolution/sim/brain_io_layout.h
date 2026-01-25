#pragma once

#include <cstddef>

namespace evolution::sim {

/**
 * @brief Shared brain I/O layout constants for sensor packing.
 * @param None.
 * @return None.
 * @note Keep values consistent across genome generation, phenotype build, and inference.
 * @warning Changing these values alters genome compatibility.
 * @threadsafe Constants are immutable.
 * @complexity O(1)
 * @throws None.
 */
inline constexpr std::size_t kBaseSensorCount = 8;

/**
 * @brief Fixed capacity for vision rays in the sensor layout.
 * @param None.
 * @return None.
 * @note Vision rays beyond this capacity are ignored; unused slots are zeroed.
 * @warning Changing this value shifts all downstream sensor indices.
 * @threadsafe Constant is immutable.
 * @complexity O(1)
 * @throws None.
 */
inline constexpr std::size_t kVisionRayCapacity = 5;

/**
 * @brief Number of social-behavior sensors appended after vision inputs.
 * @param None.
 * @return None.
 * @note This value defines the social tail length in the input vector.
 * @warning Changing this value alters brain input size for new genomes.
 * @threadsafe Constant is immutable.
 * @complexity O(1)
 * @throws None.
 */
inline constexpr std::size_t kSocialSensorCount = 12;

/**
 * @brief Total sensor input count for newly generated brains.
 * @param None.
 * @return None.
 * @note Sum of base + vision distance + vision hit-type + social tail.
 * @warning Changing this value alters genome compatibility for new brains.
 * @threadsafe Constant is immutable.
 * @complexity O(1)
 * @throws None.
 */
inline constexpr std::size_t kTotalInputCount =
    kBaseSensorCount + (2 * kVisionRayCapacity) + kSocialSensorCount;

}  // namespace evolution::sim
