#include "evolution/sim/systems/trait_analysis_system.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"

namespace evolution::sim {

TraitAnalysisSystem::TraitAnalysisSystem(double sample_interval,
                                         const std::string& output_dir) noexcept
    : sample_interval_(sample_interval) {
    
    std::filesystem::create_directories(output_dir);
    
    const std::string traits_path = output_dir + "/traits.csv";
    traits_stream_.open(traits_path, std::ios::out | std::ios::trunc);
    
    if (traits_stream_.is_open()) {
        traits_stream_ << "time,population,"
                       << "vision_range_mean,vision_range_var,vision_range_min,vision_range_max,"
                       << "vision_fov_mean,vision_fov_var,vision_fov_min,vision_fov_max,"
                       << "basal_rate_mean,basal_rate_var,basal_rate_min,basal_rate_max,"
                       << "max_energy_mean,max_energy_var,max_energy_min,max_energy_max\n";
        traits_stream_.flush();
        spdlog::info("TraitAnalysis: Opened {}", traits_path);
    } else {
        spdlog::warn("TraitAnalysis: Failed to open {}", traits_path);
    }
}

TraitAnalysisSystem::~TraitAnalysisSystem() {
    if (traits_stream_.is_open()) {
        traits_stream_.close();
    }
}

void TraitAnalysisSystem::tick(SimulationContext& context) {
    const double dt = context.fixed_dt();
    accumulator_ += dt;
    
    if (accumulator_ < sample_interval_) {
        return;
    }
    
    accumulator_ = std::fmod(accumulator_, sample_interval_);
    compute_and_log_traits(context);
}

void TraitAnalysisSystem::compute_and_log_traits(SimulationContext& context) {
    auto& registry = context.registry();
    
    std::vector<double> vision_ranges;
    std::vector<double> vision_fovs;
    std::vector<double> basal_rates;
    std::vector<double> max_energies;
    
    auto vision_view = registry.view<VisionComponent>();
    for (auto entity : vision_view) {
        const auto& vision = vision_view.get<VisionComponent>(entity);
        vision_ranges.push_back(static_cast<double>(vision.max_range));
        vision_fovs.push_back(static_cast<double>(vision.fov_radians));
    }
    
    auto metabolism_view = registry.view<MetabolismComponent>();
    for (auto entity : metabolism_view) {
        const auto& metabolism = metabolism_view.get<MetabolismComponent>(entity);
        basal_rates.push_back(metabolism.basal_rate);
        max_energies.push_back(metabolism.max_energy);
    }
    
    if (!traits_stream_.is_open()) {
        return;
    }
    
    auto vision_range_stats = compute_stats(vision_ranges);
    auto vision_fov_stats = compute_stats(vision_fovs);
    auto basal_rate_stats = compute_stats(basal_rates);
    auto max_energy_stats = compute_stats(max_energies);
    
    std::size_t population = basal_rates.size();
    
    traits_stream_ << std::fixed << std::setprecision(4)
                   << context.simulation_time() << ","
                   << population << ","
                   << vision_range_stats.mean << "," << vision_range_stats.variance << ","
                   << vision_range_stats.min << "," << vision_range_stats.max << ","
                   << vision_fov_stats.mean << "," << vision_fov_stats.variance << ","
                   << vision_fov_stats.min << "," << vision_fov_stats.max << ","
                   << basal_rate_stats.mean << "," << basal_rate_stats.variance << ","
                   << basal_rate_stats.min << "," << basal_rate_stats.max << ","
                   << max_energy_stats.mean << "," << max_energy_stats.variance << ","
                   << max_energy_stats.min << "," << max_energy_stats.max << "\n";
    traits_stream_.flush();
}

TraitAnalysisSystem::TraitStats TraitAnalysisSystem::compute_stats(
    const std::vector<double>& values) const noexcept {
    
    TraitStats stats{};
    
    if (values.empty()) {
        return stats;
    }
    
    stats.count = values.size();
    
    double sum = 0.0;
    stats.min = std::numeric_limits<double>::max();
    stats.max = std::numeric_limits<double>::lowest();
    
    for (double v : values) {
        sum += v;
        stats.min = std::min(stats.min, v);
        stats.max = std::max(stats.max, v);
    }
    
    stats.mean = sum / static_cast<double>(stats.count);
    
    double sq_diff_sum = 0.0;
    for (double v : values) {
        double diff = v - stats.mean;
        sq_diff_sum += diff * diff;
    }
    stats.variance = sq_diff_sum / static_cast<double>(stats.count);
    
    return stats;
}

}  // namespace evolution::sim
