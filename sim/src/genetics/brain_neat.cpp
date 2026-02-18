#include "evolution/genetics/brain_neat.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace evolution::genetics {

namespace {

constexpr double kSignalAbsClamp = 1.0e3;

[[nodiscard]] double sanitize_signal(double value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, -kSignalAbsClamp, kSignalAbsClamp);
}

double ActivationFunction(evolution::genome::Activation act, double value) noexcept {
    const double safe_value = sanitize_signal(value);
    switch (act) {
        case evolution::genome::Activation::Linear:
            return safe_value;
        case evolution::genome::Activation::Relu:
            return std::max(0.0, safe_value);
        case evolution::genome::Activation::Sigmoid:
            return 1.0 / (1.0 + std::exp(-std::clamp(safe_value, -60.0, 60.0)));
        case evolution::genome::Activation::Tanh:
        default:
            return std::tanh(safe_value);
    }
}

}  // namespace

BrainNeat::BrainNeat(const evolution::genome::NEAT& neat) {
    const auto* nodes = neat.nodes();
    const auto* conns = neat.conns();
    input_count_ = neat.input_count();
    output_count_ = neat.output_count();

    if (nodes == nullptr) {
        return;
    }

    nodes_.resize(nodes->size());
    std::unordered_map<std::uint32_t, std::size_t> id_to_index;
    id_to_index.reserve(nodes->size());

    for (std::size_t i = 0; i < nodes->size(); ++i) {
        const auto* node = nodes->Get(i);
        Node runtime_node{};
        runtime_node.type = node->type();
        runtime_node.activation = node->act();
        runtime_node.bias = static_cast<double>(node->bias());
        nodes_[i] = std::move(runtime_node);
        id_to_index[node->id()] = i;
        if (node->type() == evolution::genome::NodeType::Output) {
            output_indices_.push_back(i);
        }
    }

    if (conns != nullptr) {
        for (std::size_t i = 0; i < conns->size(); ++i) {
            const auto* conn = conns->Get(i);
            if (!conn->enabled()) {
                continue;
            }
            const auto out_it = id_to_index.find(conn->out());
            const auto in_it = id_to_index.find(conn->in());
            if (out_it == id_to_index.end() || in_it == id_to_index.end()) {
                continue;
            }
            Edge edge{};
            edge.source_index = in_it->second;
            edge.weight = static_cast<double>(conn->weight());
            edge.recurrent = conn->recurrent();
            nodes_[out_it->second].incoming.push_back(edge);
        }
    }

    previous_values_.assign(nodes_.size(), 0.0);
    scratch_.assign(nodes_.size(), 0.0);

    if (output_indices_.size() != output_count_) {
        output_indices_.clear();
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].type == evolution::genome::NodeType::Output) {
                output_indices_.push_back(i);
            }
        }
    }
}

void BrainNeat::evaluate(std::span<const double> inputs, std::span<double> outputs) noexcept {
    if (outputs.size() != output_count_) {
        return;
    }

    if (nodes_.empty()) {
        std::fill(outputs.begin(), outputs.end(), 0.0);
        return;
    }

    scratch_.assign(scratch_.size(), 0.0);

    // Set inputs.
    std::size_t input_written = 0;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].type == evolution::genome::NodeType::Input) {
            scratch_[i] = sanitize_signal((input_written < inputs.size()) ? inputs[input_written] : 0.0);
            ++input_written;
        }
    }

    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        if (node.type == evolution::genome::NodeType::Input) {
            continue;
        }
        double sum = node.bias;
        for (const auto& edge : node.incoming) {
            const double source_value = sanitize_signal(edge.recurrent ? previous_values_[edge.source_index]
                                                                       : scratch_[edge.source_index]);
            const double safe_weight = sanitize_signal(edge.weight);
            sum = sanitize_signal(sum + source_value * safe_weight);
        }
        scratch_[i] = sanitize_signal(ActivationFunction(node.activation, sum));
    }

    std::size_t out_index = 0;
    for (const auto node_index : output_indices_) {
        if (out_index >= outputs.size()) {
            break;
        }
        outputs[out_index++] = sanitize_signal(scratch_[node_index]);
    }
    while (out_index < outputs.size()) {
        outputs[out_index++] = 0.0;
    }

    for (std::size_t i = 0; i < previous_values_.size(); ++i) {
        previous_values_[i] = sanitize_signal(scratch_[i]);
    }
}

void BrainNeat::reset_state() noexcept {
    std::fill(previous_values_.begin(), previous_values_.end(), 0.0);
    std::fill(scratch_.begin(), scratch_.end(), 0.0);
}

}  // namespace evolution::genetics

