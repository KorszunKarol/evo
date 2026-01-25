#include "evolution/sim/systems/speciation_system.h"

#include <cmath>
#include <filesystem>
#include <iomanip>

#include <spdlog/spdlog.h>

#include "evolution/genetics/trait_extraction.h"
#include "evolution/sim/components.h"

namespace evolution::sim {

SpeciationSystem::SpeciationSystem(genetics::GenomeStorage& storage,
                                   double snapshot_interval,
                                   const std::string& output_dir) noexcept
    : storage_(storage), snapshot_interval_(snapshot_interval) {
    
    std::filesystem::create_directories(output_dir);
    
    const std::string path = output_dir + "/speciation.csv";
    speciation_stream_.open(path, std::ios::out | std::ios::trunc);
    
    if (speciation_stream_.is_open()) {
        speciation_stream_ << "time,genome_id";
        for (int i = 0; i < 16; ++i) {
            speciation_stream_ << ",f" << i;
        }
        speciation_stream_ << "\n";
        speciation_stream_.flush();
        spdlog::info("Speciation: Opened {}", path);
    } else {
        spdlog::warn("Speciation: Failed to open {}", path);
    }
}

SpeciationSystem::~SpeciationSystem() {
    if (speciation_stream_.is_open()) {
        speciation_stream_.close();
    }
}

void SpeciationSystem::tick(SimulationContext& context) {
    const double dt = context.fixed_dt();
    accumulator_ += dt;
    
    if (accumulator_ < snapshot_interval_) {
        return;
    }
    
    accumulator_ = std::fmod(accumulator_, snapshot_interval_);
    take_snapshot(context);
}

void SpeciationSystem::take_snapshot(SimulationContext& context) {
    if (!speciation_stream_.is_open()) {
        return;
    }

    auto& registry = context.registry();
    const double sim_time = context.simulation_time();

    auto view = registry.view<GenomeHandleComponent>();
    for (auto entity : view) {
        const auto& handle = view.get<GenomeHandleComponent>(entity);
        const auto* genome = storage_.get(handle.id);
        if (genome == nullptr) {
            continue;
        }

        auto features = genetics::ExtractSpeciationFeatures(*genome);

        speciation_stream_ << std::fixed << std::setprecision(4)
                           << sim_time << "," << handle.id;
        for (std::size_t i = 0; i < features.size(); ++i) {
            speciation_stream_ << "," << features[i];
        }
        speciation_stream_ << "\n";
    }
    speciation_stream_.flush();
}

}  // namespace evolution::sim
