/**
 * @file brain_neat.h
 * @brief Runtime evaluator for NEAT genomes with recurrent support.
 */

#pragma once

#include <span>
#include <vector>

#include "evolution/genetics/genome_types.h"
#include "genome_generated.h"

namespace evolution::genetics {

/**
 * @brief Compiled runtime state for executing NEAT brains deterministically.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity Construction proportional to node/edge counts.
 * @note Maintains persistent state for recurrent connections.
 * @warning Instances are not thread-safe; guard externally when shared.
 * @threadsafe @notthreadsafe.
 */
class BrainNeat {
public:
    /**
     * @brief Compile a NEAT genome into an executable runtime graph.
     * @param neat FlatBuffers NEAT table describing nodes and connections.
     * @return None.
     * @throws None.
     * @complexity O(N + E) where N = node count and E = connection count.
     * @note Constructor captures only enabled connections.
     * @warning Schema invariants must hold (unique node ids, valid references).
     * @threadsafe @notthreadsafe.
     */
    explicit BrainNeat(const evolution::genome::NEAT& neat);

    /**
     * @brief Evaluate the compiled network with supplied inputs.
     * @param inputs Sensor values; truncated or padded to match input node count.
     * @param outputs Destination buffer sized to NEAT.output_count().
     * @return None.
     * @throws None.
     * @complexity O(N + E) per invocation.
     * @note Recurrent edges consume values from the previous evaluation frame.
     * @warning Outputs span size must equal output node count; behaviour undefined otherwise.
     * @threadsafe @notthreadsafe.
     */
    void evaluate(std::span<const double> inputs, std::span<double> outputs) noexcept;

    /**
     * @brief Reset recurrent state to zeros.
     * @param None.
     * @return None.
     * @throws None.
     * @complexity O(N).
     * @note Useful when reusing runtime for a freshly spawned entity.
     * @warning Does not alter topology; only state buffers are cleared.
     * @threadsafe @notthreadsafe.
     */
    void reset_state() noexcept;

    /**
     * @brief Report the number of input nodes compiled.
     * @param None.
     * @return std::size_t Count of inputs.
     * @throws None.
     * @complexity O(1).
     * @note Value cached during construction.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::size_t input_count() const noexcept { return input_count_; }

    /**
     * @brief Report the number of output nodes.
     * @param None.
     * @return std::size_t Output count.
     * @throws None.
     * @complexity O(1).
     * @note Value cached during construction.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::size_t output_count() const noexcept { return output_count_; }

private:
    struct Edge {
        std::size_t source_index;
        double weight;
        bool recurrent;
    };

    struct Node {
        evolution::genome::NodeType type{evolution::genome::NodeType::Hidden};
        evolution::genome::Activation activation{evolution::genome::Activation::Tanh};
        double bias{0.0};
        std::vector<Edge> incoming{};
    };

    std::vector<Node> nodes_{};
    std::vector<std::size_t> output_indices_{};
    std::vector<double> previous_values_{};
    std::vector<double> scratch_{};
    std::size_t input_count_{0};
    std::size_t output_count_{0};
};

}  // namespace evolution::genetics

