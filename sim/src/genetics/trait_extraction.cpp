#include "evolution/genetics/trait_extraction.h"

#include <algorithm>
#include <cmath>

namespace evolution::genetics {

namespace {

constexpr double kMinSize = 0.1;
constexpr double kMaxSize = 10.0;
constexpr double kMinDensity = 100.0;
constexpr double kMaxDensity = 5000.0;
constexpr double kMinBrainParams = 10.0;
constexpr double kMaxBrainParams = 10000.0;

[[nodiscard]] double Normalize(double value, double min_val, double max_val) noexcept {
    const double range = max_val - min_val;
    if (range < 1e-9) {
        return 0.5;
    }
    const double clamped = std::clamp(value, min_val, max_val);
    return (clamped - min_val) / range;
}

[[nodiscard]] double ExtractBrainSize(const evolution::genome::Genome& genome) noexcept {
    double params = 0.0;
    if (const auto* cache = genome.cached()) {
        params = static_cast<double>(cache->brain_params());
    } else {
        // Fallback: estimate from brain structure
        if (genome.brain_kind() == evolution::genome::BrainKind::MLP) {
            if (const auto* mlp = genome.mlp()) {
                const auto* weights = mlp->weights();
                const auto* biases = mlp->biases();
                params = static_cast<double>((weights ? weights->size() : 0) +
                                             (biases ? biases->size() : 0));
            }
        } else if (const auto* neat = genome.neat()) {
            const auto* conns = neat->conns();
            const auto* nodes = neat->nodes();
            params = static_cast<double>((conns ? conns->size() : 0) +
                                         (nodes ? nodes->size() : 0));
        }
    }
    return Normalize(params, kMinBrainParams, kMaxBrainParams);
}

}  // namespace

std::array<double, 8> ExtractTraitVector(const evolution::genome::Genome& genome) noexcept {
    std::array<double, 8> traits{0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};

    if (const auto* body = genome.body()) {
        if (const auto* size = body->size()) {
            traits[0] = Normalize(static_cast<double>(size->x()), kMinSize, kMaxSize);
            traits[1] = Normalize(static_cast<double>(size->y()), kMinSize, kMaxSize);
            traits[2] = Normalize(static_cast<double>(size->z()), kMinSize, kMaxSize);
        }
        traits[3] = Normalize(static_cast<double>(body->mass_density()), kMinDensity, kMaxDensity);
        if (const auto* color = body->color()) {
            traits[4] = std::clamp(static_cast<double>(color->x()), 0.0, 1.0);
            traits[5] = std::clamp(static_cast<double>(color->y()), 0.0, 1.0);
            traits[6] = std::clamp(static_cast<double>(color->z()), 0.0, 1.0);
        }
    }

    traits[7] = ExtractBrainSize(genome);
    return traits;
}

std::array<double, 8> ExtractTraitVector(const evolution::genome::GenomeT& genome) noexcept {
    std::array<double, 8> traits{0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};

    if (const auto* body = genome.body.get()) {
        if (const auto* size = body->size.get()) {
            traits[0] = Normalize(static_cast<double>(size->x), kMinSize, kMaxSize);
            traits[1] = Normalize(static_cast<double>(size->y), kMinSize, kMaxSize);
            traits[2] = Normalize(static_cast<double>(size->z), kMinSize, kMaxSize);
        }
        traits[3] = Normalize(static_cast<double>(body->mass_density), kMinDensity, kMaxDensity);
        if (const auto* color = body->color.get()) {
            traits[4] = std::clamp(static_cast<double>(color->x), 0.0, 1.0);
            traits[5] = std::clamp(static_cast<double>(color->y), 0.0, 1.0);
            traits[6] = std::clamp(static_cast<double>(color->z), 0.0, 1.0);
        }
    }

    if (const auto* cache = genome.cached.get()) {
        traits[7] = Normalize(static_cast<double>(cache->brain_params), kMinBrainParams, kMaxBrainParams);
    } else {
        double params = 0.0;
        if (genome.brain_kind == evolution::genome::BrainKind::MLP && genome.mlp) {
            params = static_cast<double>(genome.mlp->weights.size() + genome.mlp->biases.size());
        } else if (genome.neat) {
            params = static_cast<double>(genome.neat->conns.size() + genome.neat->nodes.size());
        }
        traits[7] = Normalize(params, kMinBrainParams, kMaxBrainParams);
    }

    return traits;
}

double TraitCosineDistance(const std::array<double, 8>& a, const std::array<double, 8>& b) noexcept {
    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;

    for (std::size_t i = 0; i < 8; ++i) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    const double norm_a_sqrt = std::sqrt(norm_a);
    const double norm_b_sqrt = std::sqrt(norm_b);

    if (norm_a_sqrt < 1e-9 || norm_b_sqrt < 1e-9) {
        return 1.0;  // Orthogonal or zero vectors
    }

    const double cosine = dot / (norm_a_sqrt * norm_b_sqrt);
    const double clamped_cosine = std::clamp(cosine, -1.0, 1.0);
    return 1.0 - clamped_cosine;  // Distance: 0 = identical, 2 = opposite
}

}  // namespace evolution::genetics

