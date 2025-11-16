#include "evolution/sim/reproduction_system.h"

#include <algorithm>
#include <cmath>
#include <span>

#include <entt/entt.hpp>
#include <flatbuffers/flatbuffers.h>
#include <spdlog/spdlog.h>

#include "genome_generated.h"

namespace evolution::sim {

ReproductionSystem::ReproductionSystem(genetics::GenomeStorage& storage,
                                       const genetics::ReproConfig& config,
                                       std::uint64_t global_seed) noexcept
    : storage_(storage)
    , config_(config)
    , global_seed_(global_seed) {}

void ReproductionSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const double dt = context.fixed_dt();

    auto view = registry.view<ReproductionComponent,
                              GenomeHandleComponent,
                              MetabolismComponent,
                              TransformComponent>();

    std::vector<entt::entity> ready_parents;
    ready_parents.reserve(view.size_hint());

    // Collect eligible parents
    for (auto entity : view) {
        auto& repro = view.get<ReproductionComponent>(entity);
        const auto& metabolism = view.get<MetabolismComponent>(entity);

        repro.timer = std::max(0.0, repro.timer - dt);

        if (repro.timer <= 0.0 && metabolism.energy >= repro.energy_threshold) {
            ready_parents.push_back(entity);
        }
    }

        // Process each eligible parent
    for (entt::entity parent_a : ready_parents) {
        auto& repro_a = view.get<ReproductionComponent>(parent_a);
        const auto& transform_a = view.get<TransformComponent>(parent_a);
        const auto& handle_a = view.get<GenomeHandleComponent>(parent_a);

        const auto* genome_a = storage_.get(handle_a.id);
        if (genome_a == nullptr) {
            continue;
        }

        // Find candidate mates
        auto candidates = FindCandidates(registry, parent_a, transform_a.position, repro_a.mate_radius);

        if (candidates.empty()) {
            // Asexual fallback if enabled
            if (asexual_fallback_) {
                const std::uint64_t seed = genetics::derive_seed(
                    global_seed_, handle_a.id, 0x524550524F, static_cast<std::uint32_t>(parent_a));
                const auto* genome_ptr = storage_.get(handle_a.id);
                if (genome_ptr) {
                    auto genome_obj = genome_ptr->UnPack();
                    auto mutated = genetics::mutate(std::move(*genome_obj), config_, seed);
                    const genetics::GenomeId child_id = storage_.insert(std::move(mutated));

                    auto child_entity = registry.create();
                    auto result = genetics::PhenotypeBuilder::build(child_id, registry, child_entity, storage_);
                    if (result.ok) {
                        auto& child_transform = registry.get<TransformComponent>(child_entity);
                        child_transform.position = transform_a.position;
                        auto& child_repro = registry.get<ReproductionComponent>(child_entity);
                        child_repro.timer = child_repro.cooldown;

                        auto& parent_metabolism = view.get<MetabolismComponent>(parent_a);
                        parent_metabolism.energy -= repro_a.energy_threshold * 0.5;
                        repro_a.timer = repro_a.cooldown;
                    }
                }
            }
            continue;
        }

        // Evaluate preferences and select mate
        std::sort(candidates.begin(), candidates.end(),
                  [](const CandidateMate& a, const CandidateMate& b) {
                      return a.acceptance_prob > b.acceptance_prob;
                  });

        genetics::Pcg32 rng(genetics::derive_seed(
            global_seed_, handle_a.id, 0x4D415445, static_cast<std::uint32_t>(parent_a)));

        for (const auto& candidate : candidates) {
            if (candidate.acceptance_prob > 0.1 && rng.next_unit() < candidate.acceptance_prob) {
                if (AttemptReproduction(registry, parent_a, candidate.entity, rng.next_u64())) {
                    break;  // Successfully reproduced
                }
            }
        }
    }
}

std::vector<ReproductionSystem::CandidateMate> ReproductionSystem::FindCandidates(
    entt::registry& registry,
    entt::entity seeker,
    const Vec3& seeker_pos,
    double radius) const {

    std::vector<CandidateMate> candidates;
    const double radius_sq = radius * radius;

    auto view = registry.view<ReproductionComponent,
                              GenomeHandleComponent,
                              MetabolismComponent,
                              TransformComponent>();

    for (auto candidate_entity : view) {
        if (candidate_entity == seeker) {
            continue;
        }

        const auto& repro_candidate = view.get<ReproductionComponent>(candidate_entity);
        const auto& metabolism_candidate = view.get<MetabolismComponent>(candidate_entity);
        const auto& transform_candidate = view.get<TransformComponent>(candidate_entity);

        // Check cooldown and energy
        if (repro_candidate.timer > 0.0 || metabolism_candidate.energy < repro_candidate.energy_threshold) {
            continue;
        }

        // Check distance
        const Vec3 diff = transform_candidate.position - seeker_pos;
        const double dist_sq = diff.length_squared();
        if (dist_sq > radius_sq) {
            continue;
        }

        const auto& handle_seeker = view.get<GenomeHandleComponent>(seeker);
        const auto& handle_candidate = view.get<GenomeHandleComponent>(candidate_entity);

        const auto* genome_seeker = storage_.get(handle_seeker.id);
        const auto* genome_candidate = storage_.get(handle_candidate.id);

        if (genome_seeker == nullptr || genome_candidate == nullptr) {
            continue;
        }

        double acceptance = EvaluatePreference(*genome_seeker, *genome_candidate);

        // Distance weighting: closer = higher acceptance
        const double distance = std::sqrt(dist_sq);
        const double distance_factor = 1.0 / (1.0 + distance * 0.2);
        acceptance *= distance_factor;

        // Energy weighting: higher energy = higher acceptance
        const double energy_factor = metabolism_candidate.energy / repro_candidate.energy_threshold;
        acceptance *= std::min(energy_factor, 1.5);

        candidates.push_back(CandidateMate{
            .entity = candidate_entity,
            .distance = distance,
            .acceptance_prob = std::clamp(acceptance, 0.0, 1.0)
        });
    }

    return candidates;
}

double ReproductionSystem::EvaluatePreference(
    const evolution::genome::Genome& evaluator_genome,
    const evolution::genome::Genome& candidate_genome) const noexcept {

    const auto* preference = evaluator_genome.preference();
    if (preference == nullptr) {
        // Fallback: simple trait similarity
        const auto traits_eval = genetics::ExtractTraitVector(evaluator_genome);
        const auto traits_cand = genetics::ExtractTraitVector(candidate_genome);
        const double similarity = 1.0 - genetics::TraitCosineDistance(traits_eval, traits_cand);
        return std::max(0.3, similarity);  // Minimum 30% acceptance
    }

    const auto* mlp = preference->mlp();
    if (mlp == nullptr) {
        // Fallback if MLP is missing
        const auto traits_eval = genetics::ExtractTraitVector(evaluator_genome);
        const auto traits_cand = genetics::ExtractTraitVector(candidate_genome);
        const double similarity = 1.0 - genetics::TraitCosineDistance(traits_eval, traits_cand);
        return std::max(0.3, similarity);
    }

    // Extract candidate trait vector
    const auto traits = genetics::ExtractTraitVector(candidate_genome);

    // Prepare input buffer
    const std::size_t input_count = static_cast<std::size_t>(mlp->input_count());
    preference_input_buffer_.resize(std::max(input_count, traits.size()));
    std::fill(preference_input_buffer_.begin(), preference_input_buffer_.end(), 0.0);

    for (std::size_t i = 0; i < traits.size() && i < input_count; ++i) {
        preference_input_buffer_[i] = traits[i];
    }

    // Evaluate preference network
    preference_output_buffer_.resize(1);
    preference_output_buffer_[0] = 0.0;

    // Evaluate using BrainMlp directly with the MLP from preference
    const auto input_span = std::span<const double>(preference_input_buffer_.data(), input_count);
    auto output_span = std::span<double>(preference_output_buffer_.data(), 1);

    genetics::BrainMlp::Evaluate(*mlp, input_span, output_span);

    // Sigmoid to get probability
    const double raw_output = preference_output_buffer_[0];
    const double probability = 1.0 / (1.0 + std::exp(-raw_output));
    return std::clamp(probability, 0.0, 1.0);
}

bool ReproductionSystem::AttemptReproduction(
    entt::registry& registry,
    entt::entity parent_a,
    entt::entity parent_b,
    std::uint64_t seed) const {

    auto view = registry.view<ReproductionComponent,
                              GenomeHandleComponent,
                              MetabolismComponent,
                              TransformComponent>();

    const auto& handle_a = view.get<GenomeHandleComponent>(parent_a);
    const auto& handle_b = view.get<GenomeHandleComponent>(parent_b);
    const auto& transform_a = view.get<TransformComponent>(parent_a);
    const auto& repro_a = view.get<ReproductionComponent>(parent_a);
    const auto& repro_b = view.get<ReproductionComponent>(parent_b);

    const auto* genome_a = storage_.get(handle_a.id);
    const auto* genome_b = storage_.get(handle_b.id);

    if (genome_a == nullptr || genome_b == nullptr) {
        return false;
    }

    // Crossover
    auto genome_a_obj = genome_a->UnPack();
    auto genome_b_obj = genome_b->UnPack();
    auto offspring = genetics::crossover(*genome_a_obj, *genome_b_obj, config_, seed);

    // Mutate
    const std::uint64_t mutate_seed = genetics::derive_seed(
        global_seed_, handle_a.id ^ handle_b.id, 0x4D555441, 0);
    offspring = genetics::mutate(std::move(offspring), config_, mutate_seed);

    // Insert into storage
    const genetics::GenomeId child_id = storage_.insert(std::move(offspring));

    // Spawn offspring
    auto child_entity = registry.create();
    auto result = genetics::PhenotypeBuilder::build(child_id, registry, child_entity, storage_);
    if (!result.ok) {
        spdlog::warn("ReproductionSystem: failed to build phenotype: {}", result.msg);
        registry.destroy(child_entity);
        return false;
    }

    // Position near parents
    auto& child_transform = registry.get<TransformComponent>(child_entity);
    const Vec3 midpoint = (transform_a.position + view.get<TransformComponent>(parent_b).position) * 0.5;
    child_transform.position = midpoint;

    // Reset reproduction timers
    auto& child_repro = registry.get<ReproductionComponent>(child_entity);
    child_repro.timer = child_repro.cooldown;

    auto& repro_a_mut = view.get<ReproductionComponent>(parent_a);
    auto& repro_b_mut = view.get<ReproductionComponent>(parent_b);
    repro_a_mut.timer = repro_a.cooldown;
    repro_b_mut.timer = repro_b.cooldown;

    // Deduct energy costs
    auto& metabolism_a = view.get<MetabolismComponent>(parent_a);
    auto& metabolism_b = view.get<MetabolismComponent>(parent_b);
    const double cost_a = repro_a.energy_threshold * 0.4;
    const double cost_b = repro_b.energy_threshold * 0.4;
    metabolism_a.energy = std::max(0.0, metabolism_a.energy - cost_a);
    metabolism_b.energy = std::max(0.0, metabolism_b.energy - cost_b);

    // Increment offspring count for parents
    if (auto* fitness_a = registry.try_get<FitnessComponent>(parent_a)) {
        fitness_a->offspring_count++;
    }
    if (auto* fitness_b = registry.try_get<FitnessComponent>(parent_b)) {
        fitness_b->offspring_count++;
    }

    return true;
}

}  // namespace evolution::sim

