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

std::array<double, 16> ExtractSpeciationFeatures(const evolution::genome::Genome& genome) noexcept {
    std::array<double, 16> features{};
    features.fill(0.5);

    // Body morphology (0-3)
    if (const auto* body = genome.body()) {
        if (const auto* size = body->size()) {
            features[0] = Normalize(static_cast<double>(size->x()), kMinSize, kMaxSize);
            features[1] = Normalize(static_cast<double>(size->y()), kMinSize, kMaxSize);
            features[2] = Normalize(static_cast<double>(size->z()), kMinSize, kMaxSize);
        }
        features[3] = Normalize(static_cast<double>(body->mass_density()), kMinDensity, kMaxDensity);
        if (const auto* color = body->color()) {
            features[10] = std::clamp(static_cast<double>(color->x()), 0.0, 1.0);
            features[11] = std::clamp(static_cast<double>(color->y()), 0.0, 1.0);
            features[12] = std::clamp(static_cast<double>(color->z()), 0.0, 1.0);
        }
    }

    // Diet type (4) - enum, not pointer
    features[4] = static_cast<double>(genome.diet()) / 2.0; // 0=Herbivore, 1=Carnivore, 2=Omnivore

    // Metabolism from cached traits (5-6)
    if (const auto* cache = genome.cached()) {
        features[5] = Normalize(static_cast<double>(cache->basal_rate()), 0.0, 5.0);
        // Max energy is not cached, use brain_cost as proxy for complexity
        features[6] = Normalize(static_cast<double>(cache->brain_cost()), 0.0, 10.0);
    }

    // Brain structure (7-8)
    if (genome.brain_kind() == evolution::genome::BrainKind::MLP) {
        if (const auto* mlp = genome.mlp()) {
            const auto* hidden = mlp->hidden_layers();
            features[7] = Normalize(static_cast<double>(hidden ? hidden->size() : 0), 0.0, 5.0);
            if (hidden && hidden->size() > 0) {
                features[8] = Normalize(static_cast<double>(hidden->Get(0)), 4.0, 128.0);
            }
        }
    } else if (const auto* neat = genome.neat()) {
        const auto* nodes = neat->nodes();
        features[7] = Normalize(static_cast<double>(nodes ? nodes->size() : 0), 5.0, 100.0);
        features[8] = 0.5;
    }

    // Vision is runtime-only (from VisionComponent), not in genome
    // features[9] reserved for future use
    features[9] = 0.5;

    // Life stages (13-14)
    if (const auto* stages = genome.life_stages(); stages && stages->size() > 0) {
        const auto* first = stages->Get(0);
        if (first) {
            features[13] = std::clamp(static_cast<double>(first->age()), 0.0, 1.0);
            features[14] = std::clamp(static_cast<double>(first->energy_scale()), 0.0, 2.0) / 2.0;
        }
    }

    // Brain size (15)
    features[15] = ExtractBrainSize(genome);

    return features;
}

}  // namespace evolution::genetics

