#include "evolution/genetics/genome_ops.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/trait_extraction.h"

namespace evolution::genetics {

namespace {

[[nodiscard]] std::unique_ptr<evolution::genome::Vec3FT> CloneVec3(
    const std::unique_ptr<evolution::genome::Vec3FT>& vec) {
    if (!vec) {
        return nullptr;
    }
    auto clone = std::make_unique<evolution::genome::Vec3FT>();
    clone->x = vec->x;
    clone->y = vec->y;
    clone->z = vec->z;
    return clone;
}

[[nodiscard]] std::unique_ptr<evolution::genome::BodyT> CloneBody(
    const std::unique_ptr<evolution::genome::BodyT>& body) {
    if (!body) {
        return nullptr;
    }
    auto clone = std::make_unique<evolution::genome::BodyT>();
    clone->shape = body->shape;
    clone->size = CloneVec3(body->size);
    clone->mass_density = body->mass_density;
    clone->color = CloneVec3(body->color);
    return clone;
}

[[nodiscard]] std::unique_ptr<evolution::genome::MLPT> CloneMlp(
    const std::unique_ptr<evolution::genome::MLPT>& mlp) {
    if (!mlp) {
        return nullptr;
    }
    auto clone = std::make_unique<evolution::genome::MLPT>();
    clone->input_count = mlp->input_count;
    clone->output_count = mlp->output_count;
    clone->hidden_layers = mlp->hidden_layers;
    clone->weights = mlp->weights;
    clone->biases = mlp->biases;
    clone->update_rate_hz = mlp->update_rate_hz;
    return clone;
}

[[nodiscard]] std::unique_ptr<evolution::genome::NEATT> CloneNeat(
    const std::unique_ptr<evolution::genome::NEATT>& neat) {
    if (!neat) {
        return nullptr;
    }
    auto clone = std::make_unique<evolution::genome::NEATT>();
    clone->input_count = neat->input_count;
    clone->output_count = neat->output_count;
    clone->update_rate_hz = neat->update_rate_hz;

    clone->nodes.reserve(neat->nodes.size());
    for (const auto& node : neat->nodes) {
        if (!node) {
            continue;
        }
        auto clone_node = std::make_unique<evolution::genome::NeatNodeT>();
        clone_node->id = node->id;
        clone_node->type = node->type;
        clone_node->bias = node->bias;
        clone_node->act = node->act;
        clone->nodes.push_back(std::move(clone_node));
    }

    clone->conns.reserve(neat->conns.size());
    for (const auto& conn : neat->conns) {
        if (!conn) {
            continue;
        }
        auto clone_conn = std::make_unique<evolution::genome::NeatConnT>();
        clone_conn->in = conn->in;
        clone_conn->out = conn->out;
        clone_conn->weight = conn->weight;
        clone_conn->enabled = conn->enabled;
        clone_conn->innovation = conn->innovation;
        clone_conn->recurrent = conn->recurrent;
        clone->conns.push_back(std::move(clone_conn));
    }
    return clone;
}

[[nodiscard]] std::unique_ptr<evolution::genome::TraitsCacheT> CloneCache(
    const std::unique_ptr<evolution::genome::TraitsCacheT>& cache) {
    if (!cache) {
        return nullptr;
    }
    auto clone = std::make_unique<evolution::genome::TraitsCacheT>();
    clone->mass = cache->mass;
    clone->basal_rate = cache->basal_rate;
    clone->brain_cost = cache->brain_cost;
    return clone;
}

void JitterColor(evolution::genome::BodyT& body, Pcg32& rng, double sigma) noexcept {
    if (!body.color) {
        return;
    }
    const auto jitter = [&](float value) {
        const double mutated = static_cast<double>(value) + rng.normal(0.0, sigma);
        return static_cast<float>(std::clamp(mutated, 0.0, 1.0));
    };
    body.color->x = jitter(body.color->x);
    body.color->y = jitter(body.color->y);
    body.color->z = jitter(body.color->z);
}

}  // namespace

evolution::genome::GenomeT mutate(evolution::genome::GenomeT genome,
                                  const ReproConfig& config,
                                  std::uint64_t seed) noexcept {
    genome.id = 0;
    genome.parents.clear();
    Pcg32 rng(seed ^ 0xB5AD4ECEDA1CE2A9ULL);

    // Use mutator gene if present, otherwise fall back to config
    double mutate_rate_struct = config.mutate_rate_struct;
    double mutate_rate_param = config.mutate_rate_param;
    double weight_sigma = config.weight_sigma;

    if (genome.mutator) {
        mutate_rate_struct = static_cast<double>(genome.mutator->mutate_rate_struct);
        mutate_rate_param = static_cast<double>(genome.mutator->mutate_rate_param);
        weight_sigma = static_cast<double>(genome.mutator->weight_sigma);
    }

    // Body mutations
    if (genome.body) {
        if (rng.next_unit() < mutate_rate_struct) {
            const double delta = rng.normal(0.0, weight_sigma * 0.5);
            genome.body->mass_density = static_cast<float>(std::clamp(
                static_cast<double>(genome.body->mass_density) * (1.0 + delta),
                150.0,
                1500.0));
        }
        if (rng.next_unit() < mutate_rate_struct * 0.3) {
            // Shape flip mutation
            if (genome.body->shape == evolution::genome::ShapeType::Sphere) {
                genome.body->shape = rng.next_unit() < 0.5 ?
                    evolution::genome::ShapeType::CapsuleY : evolution::genome::ShapeType::Box;
            } else if (genome.body->shape == evolution::genome::ShapeType::CapsuleY) {
                genome.body->shape = rng.next_unit() < 0.5 ?
                    evolution::genome::ShapeType::Sphere : evolution::genome::ShapeType::Box;
            } else {
                genome.body->shape = rng.next_unit() < 0.5 ?
                    evolution::genome::ShapeType::Sphere : evolution::genome::ShapeType::CapsuleY;
            }
        }
        JitterColor(*genome.body, rng, weight_sigma * 0.2);
    }

    // MLP mutations
    if (genome.mlp) {
        for (auto& weight : genome.mlp->weights) {
            if (rng.next_unit() < mutate_rate_param) {
                weight = static_cast<float>(weight + rng.normal(0.0, weight_sigma));
            }
        }
        for (auto& bias : genome.mlp->biases) {
            if (rng.next_unit() < mutate_rate_param) {
                bias = static_cast<float>(bias + rng.normal(0.0, weight_sigma * 0.5));
            }
        }
        if (rng.next_unit() < mutate_rate_param * 0.1) {
            genome.mlp->update_rate_hz = static_cast<float>(
                std::clamp(static_cast<double>(genome.mlp->update_rate_hz) +
                               rng.normal(0.0, weight_sigma),
                           1.0,
                           20.0));
        }
    }

    // NEAT mutations
    if (genome.neat) {
        for (auto& conn : genome.neat->conns) {
            if (!conn) {
                continue;
            }
            if (rng.next_unit() < mutate_rate_param) {
                conn->weight = static_cast<float>(conn->weight + rng.normal(0.0, weight_sigma));
            }
            if (rng.next_unit() < mutate_rate_struct * 0.05) {
                conn->enabled = !conn->enabled;  // Toggle connection
            }
        }
        if (rng.next_unit() < mutate_rate_param * 0.1) {
            genome.neat->update_rate_hz = static_cast<float>(
                std::clamp(static_cast<double>(genome.neat->update_rate_hz) +
                               rng.normal(0.0, weight_sigma),
                           1.0,
                           20.0));
        }
    }

    // Module mutations (rare structural changes)
    if (!genome.modules.empty() && rng.next_unit() < mutate_rate_struct * 0.02) {
        // Module duplication
        const std::size_t src_idx = rng.next_u32() % genome.modules.size();
        auto dup = std::make_unique<evolution::genome::BrainModuleT>(*genome.modules[src_idx]);
        if (dup->mlp) {
            dup->mlp = std::make_unique<evolution::genome::MLPT>(*dup->mlp);
        }
        if (dup->neat) {
            dup->neat = std::make_unique<evolution::genome::NEATT>(*dup->neat);
        }
        genome.modules.push_back(std::move(dup));
    }

    // Gating network mutations
    if (genome.gating && genome.gating->mlp) {
        if (!genome.gating->mlp->weights.empty()) {
            for (auto& weight : genome.gating->mlp->weights) {
                if (rng.next_unit() < mutate_rate_param) {
                    weight = static_cast<float>(weight + rng.normal(0.0, weight_sigma * 0.3));
                }
            }
        }
        if (!genome.gating->mlp->biases.empty()) {
            for (auto& bias : genome.gating->mlp->biases) {
                if (rng.next_unit() < mutate_rate_param) {
                    bias = static_cast<float>(bias + rng.normal(0.0, weight_sigma * 0.2));
                }
            }
        }
    }

    // Preference network mutations
    if (genome.preference && genome.preference->mlp) {
        if (!genome.preference->mlp->weights.empty()) {
            for (auto& weight : genome.preference->mlp->weights) {
                if (rng.next_unit() < mutate_rate_param) {
                    weight = static_cast<float>(weight + rng.normal(0.0, weight_sigma * 0.3));
                }
            }
        }
        if (!genome.preference->mlp->biases.empty()) {
            for (auto& bias : genome.preference->mlp->biases) {
                if (rng.next_unit() < mutate_rate_param) {
                    bias = static_cast<float>(bias + rng.normal(0.0, weight_sigma * 0.2));
                }
            }
        }
    }

    genome.cached.reset();
    return genome;
}

namespace {

[[nodiscard]] std::unique_ptr<evolution::genome::MLPT> BlendMlp(
    const evolution::genome::MLPT* mlp_a,
    const evolution::genome::MLPT* mlp_b,
    Pcg32& rng) {
    if (!mlp_a && !mlp_b) {
        return nullptr;
    }
    if (!mlp_a) {
        return std::make_unique<evolution::genome::MLPT>(*mlp_b);
    }
    if (!mlp_b) {
        return std::make_unique<evolution::genome::MLPT>(*mlp_a);
    }

    if (mlp_a->input_count != mlp_b->input_count ||
        mlp_a->output_count != mlp_b->output_count ||
        mlp_a->hidden_layers.size() != mlp_b->hidden_layers.size()) {
        // Structural mismatch: choose randomly
        return rng.next_unit() < 0.5 ?
            std::make_unique<evolution::genome::MLPT>(*mlp_a) :
            std::make_unique<evolution::genome::MLPT>(*mlp_b);
    }

    auto blended = std::make_unique<evolution::genome::MLPT>();
    blended->input_count = mlp_a->input_count;
    blended->output_count = mlp_a->output_count;
    blended->hidden_layers = mlp_a->hidden_layers;
    blended->update_rate_hz = (mlp_a->update_rate_hz + mlp_b->update_rate_hz) * 0.5f;

    // Random parent per weight, averaged biases
    blended->weights.resize(mlp_a->weights.size());
    for (std::size_t i = 0; i < mlp_a->weights.size(); ++i) {
        blended->weights[i] = rng.next_unit() < 0.5 ? mlp_a->weights[i] : mlp_b->weights[i];
    }

    blended->biases.resize(mlp_a->biases.size());
    for (std::size_t i = 0; i < mlp_a->biases.size(); ++i) {
        blended->biases[i] = (mlp_a->biases[i] + mlp_b->biases[i]) * 0.5f;
    }

    return blended;
}

}  // namespace

evolution::genome::GenomeT crossover(const evolution::genome::GenomeT& a,
                                     const evolution::genome::GenomeT& b,
                                     const ReproConfig& config,
                                     std::uint64_t seed) noexcept {
    Pcg32 rng(seed ^ 0xC6BC279692B5CC83ULL);

    // Determine fitter parent (prefer higher energy/lifespan proxy via generation)
    const evolution::genome::GenomeT& fitter = (a.generation <= b.generation) ? a : b;
    const evolution::genome::GenomeT& other = (a.generation <= b.generation) ? b : a;

    evolution::genome::GenomeT child{};
    child.version = std::max(a.version, b.version);
    child.id = 0;
    child.generation = std::max(a.generation, b.generation) + 1;
    child.rng_seed = seed;
    child.parents = {a.id, b.id};

    // Body: choose fitter parent
    child.body = CloneBody(fitter.body);
    if (!child.body) {
        child.body = CloneBody(other.body);
    }

    // Brain kind: prefer fitter, fallback to other
    child.brain_kind = fitter.brain_kind;
    if (child.brain_kind == evolution::genome::BrainKind::MLP) {
        child.mlp = BlendMlp(fitter.mlp.get(), other.mlp.get(), rng);
        child.neat = nullptr;
    } else {
        // NEAT: innovation-based alignment (simplified - take fitter for now)
        child.neat = CloneNeat(fitter.neat);
        if (!child.neat) {
            child.neat = CloneNeat(other.neat);
        }
        child.mlp = nullptr;
    }

    // Module crossover: per-module selection
    if (!fitter.modules.empty() || !other.modules.empty()) {
        const std::size_t max_modules = std::max(fitter.modules.size(), other.modules.size());
        child.modules.reserve(max_modules);

        for (std::size_t i = 0; i < max_modules; ++i) {
            const bool has_a = i < fitter.modules.size() && fitter.modules[i];
            const bool has_b = i < other.modules.size() && other.modules[i];

            if (has_a && has_b) {
                // Choose parent based on energy/lifespan proxy
                const auto& chosen = rng.next_unit() < 0.6 ? fitter.modules[i] : other.modules[i];
                auto module = std::make_unique<evolution::genome::BrainModuleT>(*chosen);
                if (module->mlp) {
                    module->mlp = std::make_unique<evolution::genome::MLPT>(*module->mlp);
                }
                if (module->neat) {
                    module->neat = std::make_unique<evolution::genome::NEATT>(*module->neat);
                }
                child.modules.push_back(std::move(module));
            } else if (has_a) {
                auto module = std::make_unique<evolution::genome::BrainModuleT>(*fitter.modules[i]);
                if (module->mlp) {
                    module->mlp = std::make_unique<evolution::genome::MLPT>(*module->mlp);
                }
                if (module->neat) {
                    module->neat = std::make_unique<evolution::genome::NEATT>(*module->neat);
                }
                child.modules.push_back(std::move(module));
            } else if (has_b) {
                auto module = std::make_unique<evolution::genome::BrainModuleT>(*other.modules[i]);
                if (module->mlp) {
                    module->mlp = std::make_unique<evolution::genome::MLPT>(*module->mlp);
                }
                if (module->neat) {
                    module->neat = std::make_unique<evolution::genome::NEATT>(*module->neat);
                }
                child.modules.push_back(std::move(module));
            }
        }
    }

    // Gating: blend from both parents
    if (fitter.gating && other.gating && fitter.gating->mlp && other.gating->mlp) {
        auto blended_mlp = BlendMlp(fitter.gating->mlp.get(), other.gating->mlp.get(), rng);
        if (blended_mlp) {
            child.gating = std::make_unique<evolution::genome::BrainGatingT>(*fitter.gating);
            child.gating->mlp = std::move(blended_mlp);
        } else {
            child.gating = std::make_unique<evolution::genome::BrainGatingT>(*fitter.gating);
        }
    } else {
        child.gating = fitter.gating ? std::make_unique<evolution::genome::BrainGatingT>(*fitter.gating) :
            (other.gating ? std::make_unique<evolution::genome::BrainGatingT>(*other.gating) : nullptr);
    }

    // Preference: blend
    if (fitter.preference && other.preference && fitter.preference->mlp && other.preference->mlp) {
        auto blended_mlp = BlendMlp(fitter.preference->mlp.get(), other.preference->mlp.get(), rng);
        if (blended_mlp) {
            child.preference = std::make_unique<evolution::genome::PreferenceNetT>(*fitter.preference);
            child.preference->mlp = std::move(blended_mlp);
        } else {
            child.preference = std::make_unique<evolution::genome::PreferenceNetT>(*fitter.preference);
        }
    } else {
        child.preference = fitter.preference ?
            std::make_unique<evolution::genome::PreferenceNetT>(*fitter.preference) :
            (other.preference ? std::make_unique<evolution::genome::PreferenceNetT>(*other.preference) : nullptr);
    }

    // Mutator: inherit from fitter
    child.mutator = fitter.mutator ? std::make_unique<evolution::genome::MutatorT>(*fitter.mutator) :
        (other.mutator ? std::make_unique<evolution::genome::MutatorT>(*other.mutator) : nullptr);

    // Hotspots: merge from both parents (up to limit)
    if (!fitter.hotspots.empty() || !other.hotspots.empty()) {
        child.hotspots.reserve(std::min(fitter.hotspots.size() + other.hotspots.size(), static_cast<size_t>(5)));
        for (const auto h : fitter.hotspots) {
            child.hotspots.push_back(h);
        }
        for (const auto h : other.hotspots) {
            if (std::find(child.hotspots.begin(), child.hotspots.end(), h) == child.hotspots.end()) {
                child.hotspots.push_back(h);
            }
        }
        if (child.hotspots.size() > 5) {
            child.hotspots.resize(5);
        }
    }

    // Life stages: inherit from fitter
    if (!fitter.life_stages.empty()) {
        for (const auto& stage : fitter.life_stages) {
            if (stage) {
                auto cloned = std::make_unique<evolution::genome::LifeStageT>(*stage);
                child.life_stages.push_back(std::move(cloned));
            }
        }
    } else if (!other.life_stages.empty()) {
        for (const auto& stage : other.life_stages) {
            if (stage) {
                auto cloned = std::make_unique<evolution::genome::LifeStageT>(*stage);
                child.life_stages.push_back(std::move(cloned));
            }
        }
    }

    child.cached.reset();
    return child;
}

namespace {

[[nodiscard]] double BodyDistance(const evolution::genome::Body* body_a,
                                  const evolution::genome::Body* body_b) noexcept {
    if (!body_a || !body_b) {
        return body_a == body_b ? 0.0 : 1.0;
    }

    double dist_sq = 0.0;
    const auto* size_a = body_a->size();
    const auto* size_b = body_b->size();

    if (size_a && size_b) {
        const double dx = static_cast<double>(size_a->x()) - static_cast<double>(size_b->x());
        const double dy = static_cast<double>(size_a->y()) - static_cast<double>(size_b->y());
        const double dz = static_cast<double>(size_a->z()) - static_cast<double>(size_b->z());
        dist_sq += dx * dx + dy * dy + dz * dz;
    }

    const double density_diff = static_cast<double>(body_a->mass_density()) -
                                static_cast<double>(body_b->mass_density());
    dist_sq += density_diff * density_diff * 1e-6;  // Normalize density scale

    const double shape_penalty = (body_a->shape() == body_b->shape()) ? 0.0 : 0.5;
    dist_sq += shape_penalty * shape_penalty;

    return std::sqrt(dist_sq);
}

[[nodiscard]] double MlpDistance(const evolution::genome::MLP* mlp_a,
                                 const evolution::genome::MLP* mlp_b) noexcept {
    if (!mlp_a || !mlp_b) {
        return mlp_a == mlp_b ? 0.0 : 1.0;
    }

    if (mlp_a->input_count() != mlp_b->input_count() ||
        mlp_a->output_count() != mlp_b->output_count() ||
        mlp_a->hidden_layers()->size() != mlp_b->hidden_layers()->size()) {
        return 1.0;  // Structural mismatch
    }

    const auto* weights_a = mlp_a->weights();
    const auto* weights_b = mlp_b->weights();
    if (!weights_a || !weights_b || weights_a->size() != weights_b->size()) {
        return 1.0;
    }

    double total_diff = 0.0;
    const std::size_t weight_count = weights_a->size();
    for (std::size_t i = 0; i < weight_count; ++i) {
        const double diff = static_cast<double>(weights_a->Get(i)) -
                            static_cast<double>(weights_b->Get(i));
        total_diff += std::abs(diff);
    }

    return weight_count > 0 ? total_diff / static_cast<double>(weight_count) : 0.0;
}

[[nodiscard]] double NeatDistance(const evolution::genome::NEAT* neat_a,
                                  const evolution::genome::NEAT* neat_b,
                                  const ReproConfig& config) noexcept {
    if (!neat_a || !neat_b) {
        return neat_a == neat_b ? 0.0 : 1.0;
    }

    const auto* conns_a = neat_a->conns();
    const auto* conns_b = neat_b->conns();
    if (!conns_a || !conns_b) {
        return 1.0;
    }

    std::unordered_map<std::uint32_t, float> innovations_a;
    std::unordered_map<std::uint32_t, float> innovations_b;

    for (const auto* conn : *conns_a) {
        if (conn && conn->enabled()) {
            innovations_a[conn->innovation()] = conn->weight();
        }
    }

    for (const auto* conn : *conns_b) {
        if (conn && conn->enabled()) {
            innovations_b[conn->innovation()] = conn->weight();
        }
    }

    std::size_t excess = 0;
    std::size_t disjoint = 0;
    double weight_diff_sum = 0.0;
    std::size_t matching = 0;

    const std::uint32_t max_innov_a = innovations_a.empty() ? 0 :
        std::max_element(innovations_a.begin(), innovations_a.end(),
                        [](const auto& p1, const auto& p2) { return p1.first < p2.first; })->first;
    const std::uint32_t max_innov_b = innovations_b.empty() ? 0 :
        std::max_element(innovations_b.begin(), innovations_b.end(),
                        [](const auto& p1, const auto& p2) { return p1.first < p2.first; })->first;
    const std::uint32_t max_innov = std::max(max_innov_a, max_innov_b);

    for (const auto& [innov, weight_a] : innovations_a) {
        const auto it_b = innovations_b.find(innov);
        if (it_b != innovations_b.end()) {
            weight_diff_sum += std::abs(static_cast<double>(weight_a - it_b->second));
            ++matching;
        } else if (innov > max_innov_b) {
            ++excess;
        } else {
            ++disjoint;
        }
    }

    for (const auto& [innov, weight_b] : innovations_b) {
        if (innovations_a.find(innov) == innovations_a.end()) {
            if (innov > max_innov_a) {
                ++excess;
            } else {
                ++disjoint;
            }
        }
    }

    const double n = static_cast<double>(std::max(innovations_a.size(), innovations_b.size()));
    const double excess_term = config.neat_excess_weight * static_cast<double>(excess) / std::max(n, 1.0);
    const double disjoint_term = config.neat_disjoint_weight * static_cast<double>(disjoint) / std::max(n, 1.0);
    const double weight_term = matching > 0 ?
        config.neat_weight_diff_weight * (weight_diff_sum / static_cast<double>(matching)) : 0.0;

    return excess_term + disjoint_term + weight_term;
}

}  // namespace

double compatibility_distance(const evolution::genome::Genome& a,
                              const evolution::genome::Genome& b,
                              const ReproConfig& config) noexcept {
    if (a.id() == b.id()) {
        return 0.0;  // Identical genomes
    }

    double distance = 0.0;

    // Body distance
    const double body_dist = BodyDistance(a.body(), b.body());
    distance += config.neat_body_weight * body_dist;

    // Brain distance
    if (a.brain_kind() != b.brain_kind()) {
        distance += 2.0;  // Large penalty for different brain types
    } else if (a.brain_kind() == evolution::genome::BrainKind::MLP) {
        const double mlp_dist = MlpDistance(a.mlp(), b.mlp());
        distance += mlp_dist * 0.5;
    } else {
        const double neat_dist = NeatDistance(a.neat(), b.neat(), config);
        distance += neat_dist;
    }

    // Trait latent cosine distance
    const auto traits_a = ExtractTraitVector(a);
    const auto traits_b = ExtractTraitVector(b);
    const double trait_dist = TraitCosineDistance(traits_a, traits_b);
    distance += trait_dist * 0.3;

    // Plant trait extraction and creation
    return distance;
}

[[nodiscard]] std::unique_ptr<::evolution::genome::PlantTraitsT> ExtractPlantTraits(
    const evolution::genome::Genome* genome) noexcept;

[[nodiscard]] evolution::genome::GenomeT CreateBasePlantGenome(
    std::uint64_t seed,
    std::uint8_t species_type) {
    Pcg32 rng(seed);

    const double growth_rates[] = {2.0, 1.5, 2.5, 1.0, 2.0};
    const double max_energies[] = {20.0, 25.0, 15.0, 18.0, 22.0};
    const double seed_intervals[] = {20.0, 30.0, 15.0, 40.0, 22.0};
    const double seed_radii[] = {6.0, 5.0, 7.0, 4.0, 6.5};
    const double establish_probs[] = {0.65, 0.55, 0.70, 0.50, 0.60};

    const std::uint8_t idx = std::clamp(species_type, std::uint8_t{0}, std::uint8_t{4});

    evolution::genome::GenomeT genome;
    genome.version = 1;
    genome.generation = 0;
    genome.rng_seed = seed;
    genome.plant_traits = std::make_unique<::evolution::genome::PlantTraitsT>();

    auto& traits = *(genome.plant_traits);
    traits.growth_rate = growth_rates[idx];
    traits.max_energy = max_energies[idx];
    traits.seed_interval = seed_intervals[idx];
    traits.seed_radius = seed_radii[idx];
    traits.establish_prob = establish_probs[idx];

    const double min_energy = std::max(1.0, traits.max_energy * 0.6);
    const double seed_cost = std::max(0.5, traits.max_energy * 0.25);

    traits.seed_min_energy = min_energy;
    traits.seed_cost = seed_cost;

    GenomeStorage storage;
    const GenomeId genome_id = storage.insert(std::move(genome));

    spdlog::trace("created base plant genome: id={} species={}", genome_id, idx);

    return genome;
}

[[nodiscard]] std::unique_ptr<::evolution::genome::PlantTraitsT> ExtractPlantTraits(
    const evolution::genome::Genome* genome) noexcept {
    if (genome == nullptr) {
        spdlog::warn("ExtractPlantTraits: null genome");
        return nullptr;
    }

    if (!genome->plant_traits()) {
        spdlog::warn("ExtractPlantTraits: genome lacks plant_traits field");
        return nullptr;
    }

    auto traits = std::make_unique<::evolution::genome::PlantTraitsT>();
    genome->plant_traits()->UnPackTo(traits.get());

    return traits;
}


}  // namespace evolution::genetics
