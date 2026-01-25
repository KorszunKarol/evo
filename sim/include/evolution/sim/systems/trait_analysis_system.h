#pragma once

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Periodically samples population traits and logs statistics.
 * 
 * Tracks the evolution of physical traits over time by computing
 * Mean, Variance, Min, Max for key phenotypic traits.
 */
class TraitAnalysisSystem final : public ISystem {
public:
    struct TraitStats {
        double mean{0.0};
        double variance{0.0};
        double min{0.0};
        double max{0.0};
        std::size_t count{0};
    };

    explicit TraitAnalysisSystem(double sample_interval = 10.0,
                                  const std::string& output_dir = "telemetry") noexcept;
    ~TraitAnalysisSystem() override;

    void tick(SimulationContext& context) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "trait_analysis"; }

private:
    void compute_and_log_traits(SimulationContext& context);
    [[nodiscard]] TraitStats compute_stats(const std::vector<double>& values) const noexcept;

    double sample_interval_;
    double accumulator_{0.0};
    std::ofstream traits_stream_;
};

}  // namespace evolution::sim
