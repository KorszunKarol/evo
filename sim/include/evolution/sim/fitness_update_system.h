#pragma once

#include <string_view>

#include "evolution/sim/components.h"
#include "evolution/sim/scheduler.h"
#include "evolution/sim/simulation_context.h"

namespace evolution::sim {

/**
 * @brief Weights applied to fitness metric accumulation.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1).
 * @note Tune coefficients to balance lifespan versus energetic performance.
 * @warning Negative weights can destabilize selection heuristics; validate upstream.
 * @threadsafe @notthreadsafe Configuration is read-only after construction.
 */
struct FitnessWeights {
    double age_weight{0.2};
    double energy_weight{1.0};
    double offspring_weight{5.0};
};

/**
 * @brief Per-tick system that updates fitness metrics for entities possessing metabolism data.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(N) for N entities matching {FitnessComponent, MetabolismComponent}.
 * @note Executes after cleanup systems to ensure only live entities contribute to aggregates.
 * @warning Must execute exactly once per tick to preserve deterministic integrals.
 * @threadsafe @notthreadsafe Operates on the simulation thread without external synchronization.
 */
class FitnessUpdateSystem final : public ISystem {
public:
    /**
     * @brief Construct the system with deterministic weighting coefficients.
     * @param weights FitnessWeights Coefficient triple applied during fitness evaluation.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Coefficients remain constant for the lifetime of the system instance.
     * @warning Passing NaN coefficients yields undefined behaviour downstream.
     * @threadsafe @notthreadsafe.
     */
    explicit FitnessUpdateSystem(FitnessWeights weights = {}) noexcept;

    /**
     * @brief Update fitness metrics for all entities containing FitnessComponent and MetabolismComponent.
     * @param context SimulationContext& Shared simulation state for the current tick.
     * @return None.
     * @throws None.
     * @complexity O(N) with N equal to the number of matched entities.
     * @note Age and energy integrals increase monotonically; offspring count updated by reproduction systems.
     * @warning Expects deterministic iteration order; do not parallelize without explicit synchronization.
     * @threadsafe @notthreadsafe.
     */
    void tick(SimulationContext& context) override;

    /**
     * @brief Retrieve the human-readable system name.
     * @param None.
     * @return std::string_view Constant string literal.
     * @throws None.
     * @complexity O(1).
     * @note Used for logging and scheduler diagnostics.
     * @warning None.
     * @threadsafe Thread-safe for concurrent reads.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "fitness_update";
    FitnessWeights weights_;
};

}  // namespace evolution::sim


