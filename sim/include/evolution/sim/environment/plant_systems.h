#pragma once

#include <random>
#include <string_view>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/scheduler.h"

namespace evolution::sim {

/**
 * @brief Grows living plants by consuming soil nutrients and replenishing plant energy.
 */
class PlantGrowthSystem final : public ISystem {
public:
    explicit PlantGrowthSystem(double nutrient_to_energy = 1.0) noexcept;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    void set_conversion(double scale) noexcept { nutrient_to_energy_ = scale; }

private:
    static constexpr std::string_view name_ = "plant_growth";
    double nutrient_to_energy_{1.0};
};

/**
 * @brief Handles stochastic plant seeding and offspring creation.
 */
class PlantSeedingSystem final : public ISystem {
public:
    struct Tuning {
        unsigned int seed{0xC0FFEEU};
        double update_interval_s{1.0};
    };

    PlantSeedingSystem() noexcept;
    explicit PlantSeedingSystem(Tuning tuning) noexcept;
    explicit PlantSeedingSystem(unsigned int seed) noexcept;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    void reseed(unsigned int seed) noexcept { rng_.seed(seed); }
    [[nodiscard]] double debug_seeding_multiplier() const noexcept { return pressure_multiplier_; }

private:

    static constexpr std::string_view name_ = "plant_seeding";
    Tuning tuning_{};
    double pressure_multiplier_{1.0};
    double pressure_accumulator_{0.0};
    std::mt19937 rng_;
};

/**
 * @brief Removes plants that have remained depleted past their cleanup delay.
 */
class PlantCleanupSystem final : public ISystem {
public:
    PlantCleanupSystem() = default;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "plant_cleanup";
};

/**
 * @brief Rebuilds the plant spatial index each tick for neighborhood queries.
 */
class PlantSpatialSystem final : public ISystem {
public:
    PlantSpatialSystem() = default;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "plant_spatial";
};

}  // namespace evolution::sim
