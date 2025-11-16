/**
 * @file brain_mlp.h
 * @brief Feed-forward inference helpers for MLP genomes.
 */

#pragma once

#include <span>

#include "genome_generated.h"

namespace evolution::genetics {

/**
 * @brief Deterministic evaluator for genome-defined multilayer perceptrons.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity Construction is O(1); evaluation documented per method.
 * @note Stateless utility exposing static evaluation functions.
 * @warning Weight layout must follow schema specification (row-major per layer).
 * @threadsafe @notthreadsafe Functions operate on caller-provided buffers; synchronize externally.
 */
class BrainMlp {
public:
    /**
     * @brief Execute a forward pass through the MLP described in the genome.
     * @param mlp Immutable FlatBuffers table describing the network.
     * @param inputs Normalized sensor values; truncated or zero-padded to network input size.
     * @param outputs Mutable span receiving network outputs (must match mlp.output_count()).
     * @return None.
     * @throws None.
     * @complexity O(S) where S equals total weight count across layers.
     * @note Hidden layers use tanh activation, outputs use tanh by default.
     * @warning Outputs span must be sized to mlp.output_count(); behaviour undefined otherwise.
     * @threadsafe @notthreadsafe Callers must serialize concurrent usage.
     */
    static void Evaluate(const evolution::genome::MLP& mlp,
                         std::span<const double> inputs,
                         std::span<double> outputs) noexcept;
};

}  // namespace evolution::genetics


