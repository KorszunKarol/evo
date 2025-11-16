#pragma once

#include <string_view>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Updates soil nutrients by applying diffusion and biome-based regeneration.
 *
 * @details The system operates on the global `SoilGrid` and `BiomeMap` services stored inside
 *          the registry context. When biome map is present, uses biome-specific regeneration
 *          rates with day/night climate multiplier. Falls back to uniform regeneration if
 *          biome map is absent.
 */
class SoilSystem final : public ISystem {
public:
    /**
     * @brief Constructs the soil system.
     *
     * @param diffusion_scale double Multiplier applied to the configured diffusion rate.
     * @param regeneration_scale double Multiplier applied to the configured regeneration rate.
     * @param day_length double Day length in seconds (for day/night cycle).
     */
    explicit SoilSystem(double diffusion_scale = 1.0,
                       double regeneration_scale = 1.0,
                       double day_length = 600.0) noexcept;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    /**
     * @brief Adjusts diffusion scaling at runtime.
     *
     * @param scale double New multiplier applied to the configured diffusion rate.
     */
    void set_diffusion_scale(double scale) noexcept { diffusion_scale_ = scale; }

    /**
     * @brief Adjusts regeneration scaling at runtime.
     *
     * @param scale double New multiplier applied to the configured regeneration rate.
     */
    void set_regeneration_scale(double scale) noexcept { regeneration_scale_ = scale; }

    /**
     * @brief Sets day length for day/night cycle.
     *
     * @param day_length double Day length in seconds.
     */
    void set_day_length(double day_length) noexcept { day_length_ = day_length; }

private:
    [[nodiscard]] double compute_climate_multiplier(double sim_time) const noexcept;

    static constexpr std::string_view name_ = "soil";
    double diffusion_scale_{1.0};
    double regeneration_scale_{1.0};
    double day_length_{600.0};  // 10 minutes default
    std::array<float, 4> biome_regen_rates_{0.06F, 0.08F, 0.10F, 0.04F};  // [Plains, Forest, Wetland, Alpine]
    std::array<float, 4> biome_baselines_{4.0F, 5.0F, 6.0F, 3.0F};        // [Plains, Forest, Wetland, Alpine]
};

}  // namespace evolution::sim


