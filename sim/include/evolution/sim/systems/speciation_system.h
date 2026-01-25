#pragma once

#include <fstream>
#include <string>
#include <string_view>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Periodically exports genome feature vectors for speciation analysis.
 * 
 * Snapshots all active genomes and exports their 16-dimensional feature
 * vectors to a CSV file for clustering analysis.
 */
class SpeciationSystem final : public ISystem {
public:
    explicit SpeciationSystem(genetics::GenomeStorage& storage,
                              double snapshot_interval = 60.0,
                              const std::string& output_dir = "telemetry") noexcept;
    ~SpeciationSystem() override;

    void tick(SimulationContext& context) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "speciation"; }

private:
    void take_snapshot(SimulationContext& context);

    genetics::GenomeStorage& storage_;
    double snapshot_interval_;
    double accumulator_{0.0};
    std::ofstream speciation_stream_;
};

}  // namespace evolution::sim
