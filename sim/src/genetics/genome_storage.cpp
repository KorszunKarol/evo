#include "evolution/genetics/genome_storage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "evolution/genetics/derived_traits.h"

namespace evolution::genetics {

namespace {

constexpr float kMinRadius = 0.25f;
constexpr float kMaxRadius = 1.5f;
constexpr float kMinDensity = 250.0f;
constexpr float kMaxDensity = 900.0f;

[[nodiscard]] float RandomBetween(Pcg32& rng, float min, float max) noexcept {
    return static_cast<float>(rng.uniform(min, max));
}

[[nodiscard]] evolution::genome::Vec3FT MakeVec3(float x, float y, float z) {
    evolution::genome::Vec3FT vec{};
    vec.x = x;
    vec.y = y;
    vec.z = z;
    return vec;
}

[[nodiscard]] std::unique_ptr<evolution::genome::Vec3FT> MakeVec3Ptr(float x, float y, float z) {
    auto vec = std::make_unique<evolution::genome::Vec3FT>();
    vec->x = x;
    vec->y = y;
    vec->z = z;
    return vec;
}

void PopulateBody(evolution::genome::BodyT& body, Pcg32& rng) {
    const double choice = rng.next_unit();
    if (choice < 0.34) {
        body.shape = evolution::genome::ShapeType::Sphere;
        const float radius = RandomBetween(rng, kMinRadius, kMaxRadius);
        body.size = MakeVec3Ptr(radius, 0.0f, 0.0f);
    } else if (choice < 0.67) {
        body.shape = evolution::genome::ShapeType::CapsuleY;
        const float radius = RandomBetween(rng, kMinRadius * 0.75f, kMaxRadius * 0.8f);
        const float half_height = RandomBetween(rng, 0.4f, 2.5f);
        body.size = MakeVec3Ptr(radius, half_height, 0.0f);
    } else {
        body.shape = evolution::genome::ShapeType::Box;
        const float hx = RandomBetween(rng, 0.3f, 1.0f);
        const float hy = RandomBetween(rng, 0.3f, 1.2f);
        const float hz = RandomBetween(rng, 0.3f, 1.0f);
        body.size = MakeVec3Ptr(hx, hy, hz);
    }

    body.mass_density = RandomBetween(rng, kMinDensity, kMaxDensity);
    body.color = MakeVec3Ptr(RandomBetween(rng, 0.1f, 0.9f),
                             RandomBetween(rng, 0.1f, 0.9f),
                             RandomBetween(rng, 0.1f, 0.9f));
}

void PopulateMlp(evolution::genome::MLPT& mlp, Pcg32& rng) {
    mlp.input_count = 8;
    mlp.output_count = 4;
    mlp.hidden_layers = {16, 16};
    mlp.update_rate_hz = RandomBetween(rng, 3.0f, 10.0f);

    const auto layer_sizes = [&]() {
        std::vector<std::uint32_t> layers;
        layers.push_back(mlp.input_count);
        layers.insert(layers.end(), mlp.hidden_layers.begin(), mlp.hidden_layers.end());
        layers.push_back(mlp.output_count);
        return layers;
    }();

    std::size_t total_weights = 0;
    for (std::size_t i = 0; i + 1 < layer_sizes.size(); ++i) {
        total_weights += static_cast<std::size_t>(layer_sizes[i]) *
                         static_cast<std::size_t>(layer_sizes[i + 1]);
    }
    mlp.weights.resize(total_weights);

    std::size_t total_biases = 0;
    for (std::size_t i = 1; i < layer_sizes.size(); ++i) {
        total_biases += static_cast<std::size_t>(layer_sizes[i]);
    }
    mlp.biases.resize(total_biases);

    for (auto& weight : mlp.weights) {
        weight = static_cast<float>(rng.normal(0.0, 0.35));
    }
    for (auto& bias : mlp.biases) {
        bias = static_cast<float>(rng.normal(0.0, 0.1));
    }
}

void PopulateNeat(evolution::genome::NEATT& neat, Pcg32& rng) {
    neat.input_count = 8;
    neat.output_count = 4;
    neat.update_rate_hz = RandomBetween(rng, 3.0f, 10.0f);

    std::uint32_t next_id = 0;
    neat.nodes.reserve(static_cast<std::size_t>(neat.input_count + neat.output_count));

    for (std::uint32_t i = 0; i < neat.input_count; ++i) {
        auto node = std::make_unique<evolution::genome::NeatNodeT>();
        node->id = next_id++;
        node->type = evolution::genome::NodeType::Input;
        node->bias = 0.0f;
        node->act = evolution::genome::Activation::Linear;
        neat.nodes.push_back(std::move(node));
    }

    for (std::uint32_t i = 0; i < neat.output_count; ++i) {
        auto node = std::make_unique<evolution::genome::NeatNodeT>();
        node->id = next_id++;
        node->type = evolution::genome::NodeType::Output;
        node->bias = static_cast<float>(rng.normal(0.0, 0.1));
        node->act = evolution::genome::Activation::Tanh;
        neat.nodes.push_back(std::move(node));
    }

    std::uint32_t innovation = 1;
    for (std::uint32_t in = 0; in < neat.input_count; ++in) {
        for (std::uint32_t out = neat.input_count; out < neat.input_count + neat.output_count; ++out) {
            auto conn = std::make_unique<evolution::genome::NeatConnT>();
            conn->in = in;
            conn->out = out;
            conn->weight = static_cast<float>(rng.normal(0.0, 0.5));
            conn->enabled = true;
            conn->innovation = innovation++;
            conn->recurrent = false;
            neat.conns.push_back(std::move(conn));
        }
    }
}

flatbuffers::DetachedBuffer PackGenome(const evolution::genome::GenomeT& genome) {
    flatbuffers::FlatBufferBuilder builder(2048);
    const auto offset = evolution::genome::Genome::Pack(builder, &genome);
    builder.Finish(offset);
    return builder.Release();
}

evolution::genome::TraitsCacheT MakeTraitsCache(const DerivedTraits& traits) {
    evolution::genome::TraitsCacheT cache{};
    cache.mass = static_cast<float>(traits.mass);
    cache.basal_rate = static_cast<float>(traits.basal_rate);
    cache.brain_cost = static_cast<float>(traits.brain_cost);
    cache.brain_params = static_cast<float>(traits.brain_params);
    cache.module_count =
        static_cast<std::uint16_t>(std::min<std::size_t>(std::numeric_limits<std::uint16_t>::max(),
                                                         traits.module_count));
    cache.trait_latent.clear();
    cache.trait_latent.reserve(traits.trait_latent.size());
    for (double value : traits.trait_latent) {
        cache.trait_latent.push_back(static_cast<float>(value));
    }
    return cache;
}

}  // namespace

GenomeId HashGenomeBuffer(std::span<const std::uint8_t> buffer) noexcept {
    constexpr GenomeId kOffset = 1469598103934665603ULL;
    constexpr GenomeId kPrime = 1099511628211ULL;
    GenomeId hash = kOffset;
    for (const auto byte : buffer) {
        hash ^= static_cast<GenomeId>(byte);
        hash *= kPrime;
    }
    return hash;
}

GenomeId GenomeStorage::create_random(std::uint64_t seed) {
    evolution::genome::GenomeT genome{};
    genome.version = 1;
    genome.id = 0;
    genome.generation = 0;
    genome.rng_seed = seed;
    genome.parents.clear();

    Pcg32 rng(seed);

    genome.body = std::make_unique<evolution::genome::BodyT>();
    PopulateBody(*genome.body, rng);

    constexpr std::uint32_t kContextSize = 6;
    const std::size_t module_count = 1 + static_cast<std::size_t>(rng.next_u32() % 3);

    genome.modules.clear();
    genome.modules.reserve(module_count);

    for (std::size_t i = 0; i < module_count; ++i) {
        auto module = std::make_unique<evolution::genome::BrainModuleT>();
        const bool use_mlp = rng.next_unit() < 0.6;
        module->kind = use_mlp ? evolution::genome::BrainKind::MLP : evolution::genome::BrainKind::NEAT;
        if (use_mlp) {
            auto mlp = std::make_unique<evolution::genome::MLPT>();
            PopulateMlp(*mlp, rng);
            module->mlp = std::move(mlp);
            module->neat.reset();
        } else {
            auto neat = std::make_unique<evolution::genome::NEATT>();
            PopulateNeat(*neat, rng);
            module->neat = std::move(neat);
            module->mlp.reset();
        }
        module->plasticity = std::make_unique<evolution::genome::ModulePlasticityT>();
        auto& plasticity = *module->plasticity;
        plasticity.bias_coeffs.assign(kContextSize,
                                      static_cast<float>(rng.normal(0.0, 0.05)));
        plasticity.scale_coeffs.assign(kContextSize,
                                       static_cast<float>(rng.normal(0.0, 0.02)));
        plasticity.trace_gain = static_cast<float>(rng.uniform(0.0, 0.35));
        plasticity.trace_decay = static_cast<float>(rng.uniform(0.02, 0.15));
        genome.modules.push_back(std::move(module));
    }

    if (!genome.modules.empty()) {
        const auto* first = genome.modules.front().get();
        if (first->kind == evolution::genome::BrainKind::MLP && first->mlp) {
            genome.brain_kind = evolution::genome::BrainKind::MLP;
            genome.mlp = std::make_unique<evolution::genome::MLPT>(*first->mlp);
            genome.neat.reset();
        } else if (first->kind == evolution::genome::BrainKind::NEAT && first->neat) {
            genome.brain_kind = evolution::genome::BrainKind::NEAT;
            genome.neat = std::make_unique<evolution::genome::NEATT>(*first->neat);
            genome.mlp.reset();
        } else {
            genome.brain_kind = evolution::genome::BrainKind::MLP;
            genome.mlp = std::make_unique<evolution::genome::MLPT>();
            PopulateMlp(*genome.mlp, rng);
            genome.neat.reset();
        }
    } else {
        genome.brain_kind = evolution::genome::BrainKind::MLP;
        genome.mlp = std::make_unique<evolution::genome::MLPT>();
        PopulateMlp(*genome.mlp, rng);
        genome.neat.reset();
        auto module = std::make_unique<evolution::genome::BrainModuleT>();
        module->kind = evolution::genome::BrainKind::MLP;
        module->mlp = std::make_unique<evolution::genome::MLPT>(*genome.mlp);
        module->plasticity = std::make_unique<evolution::genome::ModulePlasticityT>();
        module->plasticity->bias_coeffs.assign(kContextSize, 0.0f);
        module->plasticity->scale_coeffs.assign(kContextSize, 0.0f);
        module->plasticity->trace_gain = 0.0f;
        module->plasticity->trace_decay = 0.0f;
        genome.modules.push_back(std::move(module));
    }

    genome.gating = std::make_unique<evolution::genome::BrainGatingT>();
    genome.gating->context_size = kContextSize;
    genome.gating->epsilon = 0.05f;
    genome.gating->mlp = std::make_unique<evolution::genome::MLPT>();
    auto& gating_mlp = *genome.gating->mlp;
    gating_mlp.input_count = kContextSize;
    gating_mlp.output_count = static_cast<std::uint32_t>(genome.modules.size());
    gating_mlp.hidden_layers = {8};
    gating_mlp.update_rate_hz = 18.0f;
    const auto gating_layers = [&]() {
        std::vector<std::uint32_t> sizes;
        sizes.push_back(gating_mlp.input_count);
        sizes.insert(sizes.end(), gating_mlp.hidden_layers.begin(), gating_mlp.hidden_layers.end());
        sizes.push_back(gating_mlp.output_count);
        return sizes;
    }();
    std::size_t gating_weight_count = 0;
    for (std::size_t i = 0; i + 1 < gating_layers.size(); ++i) {
        gating_weight_count += static_cast<std::size_t>(gating_layers[i]) *
                               static_cast<std::size_t>(gating_layers[i + 1]);
    }
    gating_mlp.weights.resize(gating_weight_count);
    for (auto& weight : gating_mlp.weights) {
        weight = static_cast<float>(rng.normal(0.0, 0.08));
    }
    std::size_t gating_bias_count = 0;
    for (std::size_t i = 1; i < gating_layers.size(); ++i) {
        gating_bias_count += static_cast<std::size_t>(gating_layers[i]);
    }
    gating_mlp.biases.resize(gating_bias_count);
    const std::size_t output_bias_offset = gating_bias_count - gating_mlp.output_count;
    for (std::size_t i = 0; i < output_bias_offset; ++i) {
        gating_mlp.biases[i] = static_cast<float>(rng.normal(0.0, 0.05));
    }
    for (std::size_t i = 0; i < gating_mlp.output_count; ++i) {
        gating_mlp.biases[output_bias_offset + i] = 1.25f;
    }

    genome.preference = std::make_unique<evolution::genome::PreferenceNetT>();
    genome.preference->trait_scale = 1.0f;
    genome.preference->mlp = std::make_unique<evolution::genome::MLPT>();
    auto& preference_mlp = *genome.preference->mlp;
    preference_mlp.input_count = 8;
    preference_mlp.output_count = 1;
    preference_mlp.hidden_layers = {12};
    preference_mlp.update_rate_hz = 6.0f;
    const auto preference_layers = [&]() {
        std::vector<std::uint32_t> sizes;
        sizes.push_back(preference_mlp.input_count);
        sizes.insert(sizes.end(), preference_mlp.hidden_layers.begin(), preference_mlp.hidden_layers.end());
        sizes.push_back(preference_mlp.output_count);
        return sizes;
    }();
    std::size_t preference_weight_count = 0;
    for (std::size_t i = 0; i + 1 < preference_layers.size(); ++i) {
        preference_weight_count += static_cast<std::size_t>(preference_layers[i]) *
                                   static_cast<std::size_t>(preference_layers[i + 1]);
    }
    preference_mlp.weights.resize(preference_weight_count);
    for (auto& weight : preference_mlp.weights) {
        weight = static_cast<float>(rng.normal(0.0, 0.07));
    }
    std::size_t preference_bias_count = 0;
    for (std::size_t i = 1; i < preference_layers.size(); ++i) {
        preference_bias_count += static_cast<std::size_t>(preference_layers[i]);
    }
    preference_mlp.biases.resize(preference_bias_count);
    const std::size_t pref_output_offset = preference_bias_count - preference_mlp.output_count;
    for (std::size_t i = 0; i < pref_output_offset; ++i) {
        preference_mlp.biases[i] = static_cast<float>(rng.normal(0.0, 0.04));
    }
    preference_mlp.biases[pref_output_offset] = -0.35f;

    genome.mutator = std::make_unique<evolution::genome::MutatorT>();
    genome.mutator->mutate_rate_struct = static_cast<float>(rng.uniform(0.02, 0.08));
    genome.mutator->mutate_rate_param = static_cast<float>(rng.uniform(0.15, 0.3));
    genome.mutator->weight_sigma = static_cast<float>(rng.uniform(0.15, 0.35));
    genome.mutator->module_duplication_rate = static_cast<float>(rng.uniform(0.005, 0.03));

    genome.hotspots.clear();
    const std::uint32_t hotspot_base = static_cast<std::uint32_t>(rng.uniform(16.0, 64.0));
    genome.hotspots.push_back(hotspot_base);
    genome.hotspots.push_back(hotspot_base + 96U);
    genome.hotspots.push_back(hotspot_base + 192U);

    genome.life_stages.clear();
    auto juvenile = std::make_unique<evolution::genome::LifeStageT>();
    juvenile->age = 0.0f;
    juvenile->gate_multipliers.assign(genome.modules.size(),
                                      static_cast<float>(rng.uniform(0.5, 0.75)));
    juvenile->energy_scale = static_cast<float>(rng.uniform(0.7, 0.9));
    juvenile->size_scale = static_cast<float>(rng.uniform(0.75, 0.9));
    juvenile->reproduction_allowed = false;
    genome.life_stages.push_back(std::move(juvenile));

    auto adult = std::make_unique<evolution::genome::LifeStageT>();
    adult->age = static_cast<float>(rng.uniform(18.0, 32.0));
    adult->gate_multipliers.assign(genome.modules.size(), 1.0f);
    adult->energy_scale = 1.0f;
    adult->size_scale = 1.0f;
    adult->reproduction_allowed = true;
    genome.life_stages.push_back(std::move(adult));

    const DerivedTraits traits = ComputeDerivedTraits(genome);
    genome.cached = std::make_unique<evolution::genome::TraitsCacheT>(MakeTraitsCache(traits));

    return insert(std::move(genome));
}

GenomeId GenomeStorage::insert(evolution::genome::GenomeT genome_obj) {
    genome_obj.id = 0;
    auto zero_buffer = PackGenome(genome_obj);
    const GenomeId hash = HashGenomeBuffer(
        std::span{zero_buffer.data(), zero_buffer.size()});
    genome_obj.id = hash;
    if (!genome_obj.cached) {
        const auto traits = ComputeDerivedTraits(genome_obj);
        genome_obj.cached = std::make_unique<evolution::genome::TraitsCacheT>(MakeTraitsCache(traits));
    } else {
        genome_obj.cached->mass = static_cast<float>(std::max(0.0, static_cast<double>(genome_obj.cached->mass)));
        genome_obj.cached->basal_rate = static_cast<float>(std::max(0.0, static_cast<double>(genome_obj.cached->basal_rate)));
        genome_obj.cached->brain_cost = static_cast<float>(std::max(0.0, static_cast<double>(genome_obj.cached->brain_cost)));
    }

    auto final_buffer = PackGenome(genome_obj);
    return insert_buffer(std::move(final_buffer));
}

const evolution::genome::Genome* GenomeStorage::get(GenomeId id) const noexcept {
    const auto it = genomes_.find(id);
    if (it == genomes_.end()) {
        return nullptr;
    }
    return flatbuffers::GetRoot<evolution::genome::Genome>(it->second.data());
}

bool GenomeStorage::contains(GenomeId id) const noexcept {
    return genomes_.find(id) != genomes_.end();
}

std::vector<GenomeId> GenomeStorage::ids() const {
    return insertion_order_;
}

void GenomeStorage::save_all() const noexcept {
    spdlog::debug("GenomeStorage::save_all() stub invoked; persistence not yet implemented.");
}

GenomeId GenomeStorage::insert_buffer(Buffer buffer) {
    const auto* genome = flatbuffers::GetRoot<evolution::genome::Genome>(buffer.data());
    const GenomeId id = genome->id();
    const auto [it, inserted] = genomes_.insert_or_assign(id, std::move(buffer));
    if (inserted) {
        insertion_order_.push_back(id);
    }
    (void)it;
    return id;
}

}  // namespace evolution::genetics

