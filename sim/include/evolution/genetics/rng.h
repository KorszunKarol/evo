/**
 * @file rng.h
 * @brief Deterministic random number utilities for genome operations.
 */

#pragma once

#include <cstdint>

namespace evolution::genetics {

/**
 * @brief PCG32-based deterministic random number generator with jump-ahead support.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity Construction is O(1).
 * @note Designed for reproducible genome mutation and crossover workflows.
 * @warning Not cryptographically secure; use solely for simulation determinism.
 * @threadsafe @notthreadsafe Guard each instance externally if accessed concurrently.
 */
class Pcg32 {
public:
    /**
     * @brief Construct a generator with seed and stream configuration.
     * @param seed 64-bit initial state seed.
     * @param stream 64-bit stream selector; defaults to 0x853c49e6748fea9bULL.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Distinct stream values yield statistically independent subsequences.
     * @warning Using the same stream across threads without synchronization is undefined.
     * @threadsafe @notthreadsafe Each instance must be confined to a single thread.
     */
    explicit Pcg32(std::uint64_t seed, std::uint64_t stream = 0x853c49e6748fea9bULL) noexcept;

    /**
     * @brief Reseed the generator to a new state/stream pair.
     * @param seed New 64-bit seed value.
     * @param stream New 64-bit stream selector.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Resets the internal sequence; previous determinism is lost.
     * @warning Callers must avoid reseeding mid-operation unless intentional.
     * @threadsafe @notthreadsafe Requires exclusive access.
     */
    void reseed(std::uint64_t seed, std::uint64_t stream = 0x853c49e6748fea9bULL) noexcept;

    /**
     * @brief Generate the next 32-bit pseudo-random unsigned integer.
     * @param None.
     * @return Next value in [0, 2^32).
     * @throws None.
     * @complexity O(1).
     * @note Implements the XSH-RR variant of PCG32.
     * @warning Sequence reproducibility hinges on deterministic call ordering.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::uint32_t next_u32() noexcept;

    /**
     * @brief Generate the next 64-bit pseudo-random unsigned integer.
     * @param None.
     * @return Next value in [0, 2^64).
     * @throws None.
     * @complexity O(1).
     * @note Combines two successive 32-bit values for extended range.
     * @warning Introduces an extra call to next_u32(); account for deterministic sequencing.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::uint64_t next_u64() noexcept;

    /**
     * @brief Sample a floating-point value uniformly within [0, 1).
     * @param None.
     * @return Double in [0, 1).
     * @throws None.
     * @complexity O(1).
     * @note Uses 53 bits of mantissa precision.
     * @warning Result excludes 1.0; clamp manually if inclusive bounds required.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] double next_unit() noexcept;

    /**
     * @brief Sample a uniform real value over [min, max).
     * @param min Inclusive lower bound.
     * @param max Exclusive upper bound (must exceed min).
     * @return Double in [min, max).
     * @throws None.
     * @complexity O(1).
     * @note Bounds are not validated for finite values; caller must ensure sanity.
     * @warning Passing max <= min yields degenerate intervals.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] double uniform(double min, double max) noexcept;

    /**
     * @brief Sample a Gaussian-distributed value using the Box-Muller transform.
     * @param mean Desired mean of the distribution.
     * @param stddev Standard deviation (must be non-negative).
     * @return Double drawn from N(mean, stddev^2).
     * @throws None.
     * @complexity O(1) amortized.
     * @note Internally caches one of the Box-Muller outputs for reuse.
     * @warning stddev < 0.0 results in undefined behaviour; validate upstream.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] double normal(double mean, double stddev) noexcept;

    /**
     * @brief Advance the generator by the specified number of steps without producing values.
     * @param delta Number of steps to skip forward.
     * @return None.
     * @throws None.
     * @complexity O(log delta).
     * @note Uses exponentiation by squaring to jump ahead efficiently.
     * @warning Passing extremely large deltas may incur floating-point precision loss when used downstream.
     * @threadsafe @notthreadsafe.
     */
    void advance(std::uint64_t delta) noexcept;

private:
    std::uint64_t state_;
    std::uint64_t increment_;
    bool has_cached_{false};
    double cached_value_{0.0};
};

/**
 * @brief Derive a deterministic seed from multiple inputs.
 * @param global_seed Global simulation seed.
 * @param genome_seed Genome-specific seed (e.g., genome ID).
 * @param op_tag Operation tag (e.g., mutation, crossover).
 * @param counter Operation counter or entity ID.
 * @return Combined seed value.
 * @throws None.
 * @complexity O(1).
 * @note Ensures deterministic but distinct seeds for different operations.
 * @warning Same inputs always produce same output.
 * @threadsafe @notthreadsafe.
 */
[[nodiscard]] std::uint64_t derive_seed(std::uint64_t global_seed,
                                         std::uint64_t genome_seed,
                                         std::uint32_t op_tag,
                                         std::uint32_t counter) noexcept;

}  // namespace evolution::genetics


