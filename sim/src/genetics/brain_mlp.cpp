#include "evolution/genetics/brain_mlp.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace evolution::genetics {

namespace {

[[nodiscard]] std::size_t LayerWeightCount(std::size_t inputs, std::size_t outputs) noexcept {
    return inputs * outputs;
}

[[nodiscard]] double Activate(double x) noexcept {
    return std::tanh(x);
}

}  // namespace

void BrainMlp::Evaluate(const evolution::genome::MLP& mlp,
                        std::span<const double> inputs,
                        std::span<double> outputs) noexcept {
    const auto* weights = mlp.weights();
    const auto* biases = mlp.biases();
    const auto* hidden_layers = mlp.hidden_layers();

    if (weights == nullptr || biases == nullptr) {
        std::fill(outputs.begin(), outputs.end(), 0.0);
        return;
    }

    std::vector<double> layer_input;
    layer_input.reserve(static_cast<std::size_t>(mlp.input_count()));
    for (std::size_t i = 0; i < static_cast<std::size_t>(mlp.input_count()); ++i) {
        layer_input.push_back(i < inputs.size() ? inputs[i] : 0.0);
    }

    std::vector<double> layer_output;
    std::size_t weight_offset = 0;
    std::size_t bias_offset = 0;
    std::size_t previous_width = layer_input.size();

    if (hidden_layers != nullptr) {
        for (std::size_t layer_idx = 0; layer_idx < hidden_layers->size(); ++layer_idx) {
            const std::size_t width = static_cast<std::size_t>(hidden_layers->Get(layer_idx));
            layer_output.assign(width, 0.0);

            for (std::size_t neuron = 0; neuron < width; ++neuron) {
                double sum = biases->Get(static_cast<std::size_t>(bias_offset + neuron));
                for (std::size_t input_idx = 0; input_idx < previous_width; ++input_idx) {
                    const std::size_t index = weight_offset + neuron * previous_width + input_idx;
                    sum += layer_input[input_idx] * weights->Get(static_cast<std::size_t>(index));
                }
                layer_output[neuron] = Activate(sum);
            }

            weight_offset += LayerWeightCount(previous_width, width);
            bias_offset += width;
            layer_input = layer_output;
            previous_width = width;
        }
    }

    const std::size_t output_count = static_cast<std::size_t>(mlp.output_count());
    if (outputs.size() != output_count) {
        return;
    }
    layer_output.assign(output_count, 0.0);

    for (std::size_t neuron = 0; neuron < output_count; ++neuron) {
        double sum = biases->Get(static_cast<std::size_t>(bias_offset + neuron));
        for (std::size_t input_idx = 0; input_idx < previous_width; ++input_idx) {
            const std::size_t index = weight_offset + neuron * previous_width + input_idx;
            sum += layer_input[input_idx] * weights->Get(static_cast<std::size_t>(index));
        }
        layer_output[neuron] = Activate(sum);
    }

    std::copy(layer_output.begin(), layer_output.end(), outputs.begin());
}

}  // namespace evolution::genetics


