#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "evolution/sim/scheduler.h"

namespace evolution::genetics {
class GenomeStorage;
}

namespace evolution::sim {

struct PopulationConfig {
    std::size_t min_viable_population{10};
    std::size_t max_carry_capacity{500};
    std::size_t hard_cap{1000};
    std::size_t target_cap{800};
    std::size_t rescue_batch_size{16};
    std::size_t extinction_grace_ticks{180};
    std::size_t monitor_ema_window{30};
    double soil_depletion_threshold{0.2};
    double min_seeding_multiplier{0.1};
    std::size_t plant_soft_capacity{6000};
    std::size_t plant_high_pressure_capacity{12000};
    double plant_pressure_gain{1.0};
    double min_plant_seeding_multiplier{0.02};
    double max_plant_seeding_multiplier{1.0};
    double density_feedback_gain{1.2};
    double herbivore_pressure_gain{0.8};
    double seeding_update_interval_s{1.0};
    double world_area_override{0.0};
};

struct PopulationSnapshot {
    std::size_t creature_count{0};
    std::size_t herbivore_count{0};
    std::size_t carnivore_count{0};
    std::size_t plant_count{0};

    double creature_density{0.0};
    double herbivore_to_plant_ratio{0.0};
    double carnivore_to_herbivore_ratio{0.0};

    double ema_creature_count{0.0};
    double population_stability_index{0.0};

    bool below_min_viable{false};
    bool above_soft_capacity{false};
    bool above_hard_cap{false};
};

struct PopulationEventCounters {
    std::uint64_t births_total{0};
    std::uint64_t deaths_total{0};
    std::uint64_t rescues_total{0};
    std::uint64_t culls_total{0};
};

struct PopulationMonitor {
    PopulationConfig config{};
    PopulationSnapshot latest{};
    std::size_t below_min_streak_ticks{0};

    double ema_count{0.0};
    double ema_count_sq{0.0};
    bool ema_initialized{false};

    std::uint64_t update_tick{0};
    std::uint64_t rescue_events{0};
    std::uint64_t cull_events{0};
};

class PopulationSystem final : public ISystem {
public:
    PopulationSystem(genetics::GenomeStorage& storage,
                     const PopulationConfig& config,
                     std::uint64_t genome_seed) noexcept;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    static constexpr std::string_view name_ = "population";

    genetics::GenomeStorage& storage_;
    PopulationConfig config_{};
    std::uint64_t genome_seed_{0};
    std::uint64_t local_tick_{0};

    void update_snapshot(SimulationContext& context, PopulationMonitor& monitor);
    void maybe_rescue(SimulationContext& context, PopulationMonitor& monitor);
    void maybe_cull(SimulationContext& context, PopulationMonitor& monitor);
};

}  // namespace evolution::sim
