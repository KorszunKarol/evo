/**
 * @file brain_inference_system.h
 * @brief Executes neural controllers and writes actuation intents.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "evolution/genetics/brain_neat.h"
#include "evolution/genetics/brain_mlp.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/components.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief System responsible for running per-entity brain inference.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity Construction is O(1).
 * @note Maintains runtime caches for NEAT recurrent state.
 * @warning Requires deterministic scheduling before MotorSystem.
 * @threadsafe @notthreadsafe.
 */
class BrainInferenceSystem final : public ISystem {
public:
    /**
     * @brief Construct the system with a reference to genome storage.
     * @param storage genetics::GenomeStorage& Source of serialized genomes.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Storage must outlive the system.
     * @warning Passing a dangling storage reference results in undefined behaviour.
     * @threadsafe @notthreadsafe.
     */
    explicit BrainInferenceSystem(genetics::GenomeStorage& storage) noexcept;

    /**
     * @brief Evaluate brains for entities whose update interval has elapsed.
     * @param context SimulationContext& Tick metadata and registry access.
     * @return None.
     * @throws None.
     * @complexity O(E × W) where E = active brains and W = weights/edges per brain.
     * @note Only entities with BrainComponent and ActuationComponent are processed.
     * @warning Must run on the simulation thread before motor integration.
     * @threadsafe @notthreadsafe.
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Retrieve the system identifier.
     * @param None.
     * @return std::string_view Literal name of the system.
     * @throws None.
     * @complexity O(1).
     * @note Used for diagnostics.
     * @warning None.
     * @threadsafe Thread-safe for concurrent reads.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    struct NeatSlot {
        genetics::GenomeId genome_id{};
        std::uint32_t module_index{};
        genetics::BrainNeat runtime;
    };

    [[nodiscard]] std::span<double> ensure_output_buffer(std::size_t count);
    [[nodiscard]] genetics::BrainNeat& fetch_neat_runtime(genetics::GenomeId id,
                                                          std::uint32_t module_index,
                                                          const evolution::genome::NEAT& neat);

    static constexpr std::string_view name_ = "brain_inference";
    genetics::GenomeStorage& storage_;
    std::vector<NeatSlot> neat_slots_{};
    std::vector<double> input_buffer_{};
    std::vector<double> output_buffer_{};
    std::vector<double> context_buffer_{};
    std::vector<double> gating_buffer_{};
    std::vector<double> module_buffer_{};
};

}  // namespace evolution::sim


