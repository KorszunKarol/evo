#include "evolution/genetics/genome_ops.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "evolution/genetics/innovation_database.h"
#include "evolution/genetics/morphology_ops.h"
#include "evolution/genetics/mutation_ops.h"
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

[[nodiscard]] std::unique_ptr<evolution::genome::JointT> CloneJoint(
    const std::unique_ptr<evolution::genome::JointT>& joint) {
    if (!joint) {
        return nullptr;
    }
    auto clone = std::make_unique<evolution::genome::JointT>();
    clone->type = joint->type;
    clone->axis = CloneVec3(joint->axis);
    clone->limits = CloneVec3(joint->limits);
    clone->anchor = CloneVec3(joint->anchor);
    return clone;
}

[[nodiscard]] std::unique_ptr<evolution::genome::BodyNodeT> CloneBodyNode(
    const std::unique_ptr<evolution::genome::BodyNodeT>& node) {
    if (!node) {
        return nullptr;
    }
    auto clone = std::make_unique<evolution::genome::BodyNodeT>();
    clone->shape = node->shape;
    clone->size = CloneVec3(node->size);
    clone->mass_density = node->mass_density;
    clone->color = CloneVec3(node->color);
    clone->joint_to_parent = CloneJoint(node->joint_to_parent);
    clone->transform = CloneVec3(node->transform);
    
    clone->children.reserve(node->children.size());
    for (const auto& child : node->children) {
        clone->children.push_back(CloneBodyNode(child));
    }
    
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

void JitterColor(evolution::genome::BodyNodeT& body, Pcg32& rng, double sigma) noexcept {
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
    
    // Recurse for children
    for (auto& child : body.children) {
        if (child) {
            JitterColor(*child, rng, sigma);
        }
    }
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
        
        // Structural morphology mutations (limb add/remove/modify)
        MorphologyConstraints morph_constraints{};
        apply_morphology_mutations(
            *genome.body, rng,
            mutate_rate_struct * 0.1,   // add_prob
            mutate_rate_struct * 0.3,   // modify_prob
            mutate_rate_struct * 0.05,  // remove_prob
            morph_constraints);
    }
    
    // Diet mutation (rare - ~1% chance)
    if (rng.next_unit() < 0.01) {
        // Flip diet type
        if (genome.diet == evolution::genome::DietPreference::Herbivore) {
            genome.diet = evolution::genome::DietPreference::Carnivore;
        } else if (genome.diet == evolution::genome::DietPreference::Carnivore) {
            genome.diet = evolution::genome::DietPreference::Herbivore;
        }
    }
    
    // Speed trait mutation (affects movement speed)
    if (rng.next_unit() < mutate_rate_param) {
        const double delta = rng.normal(0.0, weight_sigma * 0.3);
        genome.speed_trait = static_cast<float>(std::clamp(
            static_cast<double>(genome.speed_trait) + delta,
            0.5,
            2.0));
    }
    
    // Attack reach mutation (carnivore predation range)
    if (rng.next_unit() < mutate_rate_param * 0.5) {
        const double delta = rng.normal(0.0, weight_sigma * 0.2);
        genome.attack_reach = static_cast<float>(std::clamp(
            static_cast<double>(genome.attack_reach) + delta,
            1.0,
            3.0));
    }
    
    // Attack power mutation (energy drain rate)
    if (rng.next_unit() < mutate_rate_param * 0.5) {
        const double delta = rng.normal(0.0, weight_sigma * 2.0);
        genome.attack_power = static_cast<float>(std::clamp(
            static_cast<double>(genome.attack_power) + delta,
            3.0,
            15.0));
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

evolution::genome::GenomeT mutate(evolution::genome::GenomeT genome,
                                  const ReproConfig& config,
                                  std::uint64_t seed,
                                  InnovationDatabase& innovations) noexcept {
    genome = mutate(std::move(genome), config, seed);
    
    if (genome.neat) {
        Pcg32 rng(seed ^ 0x5354525543544D55ULL);
        
        StructuralMutationConfig struct_config{};
        struct_config.add_node_prob = config.mutate_rate_struct * 0.3;
        struct_config.add_conn_prob = config.mutate_rate_struct * 0.5;
        struct_config.delete_conn_prob = config.mutate_rate_struct * 0.1;
        struct_config.weight_init_sigma = config.weight_sigma;
        struct_config.allow_recurrent = true;
        
        apply_structural_mutations(*genome.neat, innovations, rng, struct_config);
    }
    
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

[[nodiscard]] std::unique_ptr<evolution::genome::NEATT> CrossoverNeat(
    const evolution::genome::NEATT* fitter_neat,
    const evolution::genome::NEATT* other_neat,
    Pcg32& rng) {
    if (!fitter_neat && !other_neat) {
        return nullptr;
    }
    if (!fitter_neat) {
        return CloneNeat(std::make_unique<evolution::genome::NEATT>(*other_neat));
    }
    if (!other_neat) {
        return CloneNeat(std::make_unique<evolution::genome::NEATT>(*fitter_neat));
    }

    auto child = std::make_unique<evolution::genome::NEATT>();
    child->input_count = fitter_neat->input_count;
    child->output_count = fitter_neat->output_count;
    child->update_rate_hz = (fitter_neat->update_rate_hz + other_neat->update_rate_hz) * 0.5f;

    std::unordered_map<std::uint32_t, const evolution::genome::NeatConnT*> fitter_conns;
    std::unordered_map<std::uint32_t, const evolution::genome::NeatConnT*> other_conns;
    std::uint32_t max_fitter_innov = 0;
    std::uint32_t max_other_innov = 0;

    for (const auto& conn : fitter_neat->conns) {
        if (conn) {
            fitter_conns[conn->innovation] = conn.get();
            max_fitter_innov = std::max(max_fitter_innov, conn->innovation);
        }
    }
    for (const auto& conn : other_neat->conns) {
        if (conn) {
            other_conns[conn->innovation] = conn.get();
            max_other_innov = std::max(max_other_innov, conn->innovation);
        }
    }

    std::unordered_set<std::uint32_t> all_innovations;
    for (const auto& [innov, _] : fitter_conns) {
        all_innovations.insert(innov);
    }
    for (const auto& [innov, _] : other_conns) {
        all_innovations.insert(innov);
    }

    std::unordered_set<std::uint32_t> needed_nodes;
    
    for (std::uint32_t innov : all_innovations) {
        const auto fitter_it = fitter_conns.find(innov);
        const auto other_it = other_conns.find(innov);
        
        const evolution::genome::NeatConnT* chosen = nullptr;
        
        if (fitter_it != fitter_conns.end() && other_it != other_conns.end()) {
            chosen = rng.next_unit() < 0.5 ? fitter_it->second : other_it->second;
        } else if (fitter_it != fitter_conns.end()) {
            chosen = fitter_it->second;
        } else if (innov <= max_fitter_innov && other_it != other_conns.end()) {
            continue;
        } else if (other_it != other_conns.end()) {
            continue;
        }
        
        if (chosen) {
            auto new_conn = std::make_unique<evolution::genome::NeatConnT>();
            new_conn->in = chosen->in;
            new_conn->out = chosen->out;
            new_conn->weight = chosen->weight;
            new_conn->innovation = chosen->innovation;
            new_conn->recurrent = chosen->recurrent;
            
            if (!chosen->enabled) {
                new_conn->enabled = rng.next_unit() < 0.25;
            } else {
                new_conn->enabled = true;
            }
            
            needed_nodes.insert(new_conn->in);
            needed_nodes.insert(new_conn->out);
            child->conns.push_back(std::move(new_conn));
        }
    }
    
    std::unordered_map<std::uint32_t, const evolution::genome::NeatNodeT*> fitter_nodes;
    std::unordered_map<std::uint32_t, const evolution::genome::NeatNodeT*> other_nodes;
    
    for (const auto& node : fitter_neat->nodes) {
        if (node) {
            fitter_nodes[node->id] = node.get();
        }
    }
    for (const auto& node : other_neat->nodes) {
        if (node) {
            other_nodes[node->id] = node.get();
        }
    }
    
    for (std::uint32_t node_id : needed_nodes) {
        const evolution::genome::NeatNodeT* chosen = nullptr;
        
        const auto fitter_it = fitter_nodes.find(node_id);
        const auto other_it = other_nodes.find(node_id);
        
        if (fitter_it != fitter_nodes.end() && other_it != other_nodes.end()) {
            chosen = rng.next_unit() < 0.5 ? fitter_it->second : other_it->second;
        } else if (fitter_it != fitter_nodes.end()) {
            chosen = fitter_it->second;
        } else if (other_it != other_nodes.end()) {
            chosen = other_it->second;
        }
        
        if (chosen) {
            auto new_node = std::make_unique<evolution::genome::NeatNodeT>();
            new_node->id = chosen->id;
            new_node->type = chosen->type;
            new_node->bias = chosen->bias;
            new_node->act = chosen->act;
            child->nodes.push_back(std::move(new_node));
        }
    }
    
    std::sort(child->nodes.begin(), child->nodes.end(),
              [](const auto& a, const auto& b) {
                  if (!a || !b) return false;
                  if (a->type != b->type) {
                      return static_cast<int>(a->type) < static_cast<int>(b->type);
                  }
                  return a->id < b->id;
              });
    
    return child;
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
    child.body = CloneBodyNode(fitter.body);
    if (!child.body) {
        child.body = CloneBodyNode(other.body);
    }
    
    // Diet: inherit from fitter parent (with small chance of using other parent)
    // This creates reproductive isolation between dietary types
    if (rng.next_unit() < 0.85) {
        child.diet = fitter.diet;
    } else {
        child.diet = other.diet;
    }
    
    // Inherit combat traits: blend attack reach and power from both parents
    child.attack_reach = static_cast<float>(
        static_cast<double>(fitter.attack_reach) * 0.6 +
        static_cast<double>(other.attack_reach) * 0.4);
    child.attack_power = static_cast<float>(
        static_cast<double>(fitter.attack_power) * 0.6 +
        static_cast<double>(other.attack_power) * 0.4);
    child.speed_trait = static_cast<float>(
        static_cast<double>(fitter.speed_trait) * 0.6 +
        static_cast<double>(other.speed_trait) * 0.4);

    // Brain kind: prefer fitter, fallback to other
    child.brain_kind = fitter.brain_kind;
    if (child.brain_kind == evolution::genome::BrainKind::MLP) {
        child.mlp = BlendMlp(fitter.mlp.get(), other.mlp.get(), rng);
        child.neat = nullptr;
    } else {
        // NEAT: proper gene alignment by innovation number
        child.neat = CrossoverNeat(fitter.neat.get(), other.neat.get(), rng);
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
        child.hotspots.reserve(std::min(fitter.hotspots.size() + other.hotspots.size(), 5UL));
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

[[nodiscard]] double BodyDistance(const evolution::genome::BodyNode* body_a,
                                  const evolution::genome::BodyNode* body_b) noexcept {
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
    
    // Simple structural diff for children count
    double children_diff = 0.0;
    if (body_a->children() && body_b->children()) {
        children_diff = static_cast<double>(body_a->children()->size()) - static_cast<double>(body_b->children()->size());
    } else if (body_a->children()) {
        children_diff = static_cast<double>(body_a->children()->size());
    } else if (body_b->children()) {
        children_diff = static_cast<double>(body_b->children()->size());
    }
    dist_sq += children_diff * children_diff * 0.1;

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
    
    // Diet distance: different diet types add significant penalty
    // This encourages reproductive isolation between herbivores and carnivores
    if (a.diet() != b.diet()) {
        distance += 1.5;  // Large penalty for different diet types
    }

    return distance;
}

}  // namespace evolution::genetics


