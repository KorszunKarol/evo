/**
 * @file reproduction_system.h
 * @brief Preference-driven mate selection and reproduction system.
 */

#pragma once

#include <memory>
#include <string_view>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/phenotype_builder.h"
#include "evolution/genetics/trait_extraction.h"
#include "evolution/genetics/brain_mlp.h"
#include "evolution/sim/components.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

class TelemetrySystem;

/**
 * @brief System that handles sexual reproduction with preference-based mate selection.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity Construction is O(1).
 * @note Uses PreferenceNet when available, falls back to energy/cooldown rules.
 * @warning Requires GenomeStorage and deterministic RNG seeding.
 * @threadsafe @notthreadsafe.
 */
class ReproductionSystem final : public ISystem {
public:
    /**
     * @brief Construct reproduction system with genome storage and configuration.
     * @param storage Reference to genome storage (must outlive system).
     * @param config Reproduction configuration parameters.
     * @param global_seed Global seed for deterministic operations.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Storage must outlive system instance.
     * @warning Passing dangling storage reference results in undefined behaviour.
     * @threadsafe @notthreadsafe.
     */
    explicit ReproductionSystem(genetics::GenomeStorage& storage,
                                 const genetics::ReproConfig& config,
                                 std::uint64_t global_seed) noexcept;
    


    /**
     * @brief Process reproduction opportunities for eligible entities.
     * @param context Simulation context with registry and timing.
     * @return None.
     * @throws None.
     * @complexity O(N × M) where N = eligible entities, M = candidates in radius.
     * @note Only processes entities with ReproductionComponent and GenomeHandleComponent.
     * @warning Must run after MetabolismSystem to ensure energy thresholds accurate.
     * @threadsafe @notthreadsafe.
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Retrieve system identifier.
     * @param None.
     * @return std::string_view Literal name.
     * @throws None.
     * @complexity O(1).
     * @note Used for diagnostics.
     * @warning None.
     * @threadsafe Thread-safe for concurrent reads.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    /**
     * @brief Enable or disable asexual fallback reproduction.
     * @param enabled Whether asexual reproduction is allowed when no mate found.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Default is false (sexual reproduction only).
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    void set_asexual_fallback(bool enabled) noexcept { asexual_fallback_ = enabled; }

private:
    struct CandidateMate {
        entt::entity entity{entt::null};
        double distance{0.0};
        double acceptance_prob{0.0};
    };

    [[nodiscard]] std::vector<CandidateMate> FindCandidates(
        entt::registry& registry,
        entt::entity seeker,
        const Vec3& seeker_pos,
        double radius) const;

    [[nodiscard]] double EvaluatePreference(
        const evolution::genome::Genome& evaluator_genome,
        const evolution::genome::Genome& candidate_genome) const noexcept;

    [[nodiscard]] bool AttemptReproduction(
        double sim_time,
        entt::registry& registry,
        entt::entity parent_a,
        entt::entity parent_b,
        std::uint64_t seed) const;

    static constexpr std::string_view name_ = "reproduction";
    genetics::GenomeStorage& storage_;
    genetics::ReproConfig config_;
    std::uint64_t global_seed_;
    bool asexual_fallback_{false};
    std::vector<CandidateMate> candidate_buffer_{};
    mutable std::vector<double> preference_input_buffer_{};
    mutable std::vector<double> preference_output_buffer_{};
    TelemetrySystem* telemetry_{nullptr};

public:
    void set_telemetry(TelemetrySystem* telemetry) noexcept { telemetry_ = telemetry; }
};

}  // namespace evolution::sim

