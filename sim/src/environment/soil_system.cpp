#include "evolution/sim/environment/soil_system.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "evolution/sim/environment/soil_volume.h"

namespace evolution::sim {

SoilSystem::SoilSystem(double diffusion_scale, double regeneration_scale, double day_length) noexcept
    : diffusion_scale_(diffusion_scale),
      regeneration_scale_(regeneration_scale),
      day_length_(day_length) {}

double SoilSystem::compute_climate_multiplier(double sim_time) const noexcept {
    if (day_length_ <= 0.0) {
        return 1.0;
    }
    // Day/night multiplier: lerp(0.5, 1.2, 0.5*(1+cos(2π t / T_day)))
    const double phase = 2.0 * std::numbers::pi_v<double> * sim_time / day_length_;
    const double cosine = std::cos(phase);
    const double normalized = 0.5 * (1.0 + cosine);  // [0, 1]
    return 0.5 + normalized * 0.7;  // [0.5, 1.2]
}

void SoilSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const double dt = context.fixed_dt();

    // 1. Update Legacy 2D SoilGrid (if present)
    // This keeps existing tests and logic working until full migration
    if (registry.ctx().contains<SoilGrid>()) {
        auto& soil = registry.ctx().get<SoilGrid>();
        
        if (diffusion_scale_ > 0.0) {
            soil.diffuse(dt * diffusion_scale_);
        }

        // Use biome-based regeneration if BiomeMap is available
        if (regeneration_scale_ > 0.0) {
            const auto* biome_map = registry.ctx().find<BiomeMap>();
            if (biome_map != nullptr) {
                const double climate_mult = compute_climate_multiplier(context.simulation_time());
                const double effective_mult = climate_mult * regeneration_scale_;
                
                // Apply biome-specific regeneration rates
                std::array<float, 4> scaled_regen_rates;
                for (std::size_t i = 0; i < 4; ++i) {
                    scaled_regen_rates[i] = biome_regen_rates_[i] * static_cast<float>(effective_mult);
                }
                
                soil.regenerate_by_biome(dt, *biome_map, scaled_regen_rates, biome_baselines_, 1.0);
            } else {
                // Fallback to uniform regeneration
                soil.regenerate(dt * regeneration_scale_);
            }
        }
    }

    // 2. Update New 3D SoilVolume (if present)
    if (registry.ctx().contains<SoilVolume>()) {
        auto& volume = registry.ctx().get<SoilVolume>();
        if (diffusion_scale_ > 0.0) {
            volume.diffuse(math::Fixed64(dt * diffusion_scale_));
        }
        
        if (regeneration_scale_ > 0.0) {
            const auto* biome_map = registry.ctx().find<BiomeMap>();
            const double climate_mult = compute_climate_multiplier(context.simulation_time());
            const double effective_mult = climate_mult * regeneration_scale_;
            volume.regenerate(math::Fixed64(dt), biome_map, effective_mult);
        }
    }
}

}  // namespace evolution::sim
