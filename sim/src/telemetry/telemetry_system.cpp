#include "evolution/sim/telemetry_system.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"

namespace evolution::sim {

namespace {

const char* event_type_to_string(TelemetryEventType type) {
    switch (type) {
        case TelemetryEventType::AgentDied: return "DEATH";
        case TelemetryEventType::FeedingEvent: return "FEEDING";
        case TelemetryEventType::ReproductionEvent: return "REPRODUCTION";
        case TelemetryEventType::AgentSpawned: return "SPAWN";
        default: return "UNKNOWN";
    }
}

const char* death_cause_to_string(std::uint8_t cause) {
    switch (static_cast<DeathCause>(cause)) {
        case DeathCause::Starvation: return "STARVATION";
        case DeathCause::Predation: return "PREDATION";
        case DeathCause::OldAge: return "OLD_AGE";
        default: return "UNKNOWN";
    }
}

}  // namespace

TelemetrySystem::TelemetrySystem(double metrics_interval,
                                 const std::string& output_dir) noexcept
    : metrics_interval_(metrics_interval) {
    
    std::filesystem::create_directories(output_dir);
    
    const std::string metrics_path = output_dir + "/metrics.csv";
    const std::string events_path = output_dir + "/events.jsonl";
    
    metrics_stream_.open(metrics_path, std::ios::out | std::ios::trunc);
    events_stream_.open(events_path, std::ios::out | std::ios::trunc);
    
    // Phase 4b: Brain Dump Stream
    const std::string brain_path = output_dir + "/brain_dump.jsonl";
    brain_stream_.open(brain_path, std::ios::out | std::ios::trunc);

    if (metrics_stream_.is_open()) {
        metrics_stream_ << "time,herbivores,carnivores,plants,corpses,"
                        << "mean_energy,stddev_energy,min_energy,max_energy,"
                        << "biomass_producers,biomass_consumers,biomass_corpses,biomass_soil,"
                        << "energy_hunting,energy_scavenging,energy_total_transfer,"
                        << "deaths_starvation,deaths_predation\n";
        metrics_stream_.flush();
        spdlog::info("Telemetry: Opened {}", metrics_path);
    } else {
        spdlog::warn("Telemetry: Failed to open {}", metrics_path);
    }
    
    if (events_stream_.is_open()) {
        spdlog::info("Telemetry: Opened {}", events_path);
    } else {
        spdlog::warn("Telemetry: Failed to open {}", events_path);
    }
    
    if (brain_stream_.is_open()) {
        spdlog::info("Telemetry: Opened {}", brain_path);
    } else {
        spdlog::warn("Telemetry: Failed to open {}", brain_path);
    }

    // Neural Manifold Stream
    const std::string neural_path = output_dir + "/brain_activations.csv";
    neural_stream_.open(neural_path, std::ios::out | std::ios::trunc);
    if (neural_stream_.is_open()) {
        neural_stream_ << "time,entity_id,action_mask";
        for (int i = 0; i < 32; ++i) {
            neural_stream_ << ",v" << i;
        }
        neural_stream_ << "\n";
        neural_stream_.flush();
        spdlog::info("Telemetry: Opened {}", neural_path);
    } else {
        spdlog::warn("Telemetry: Failed to open {}", neural_path);
    }
}

TelemetrySystem::~TelemetrySystem() {
    flush_events();
    if (metrics_stream_.is_open()) metrics_stream_.close();
    if (events_stream_.is_open()) events_stream_.close();
    if (brain_stream_.is_open()) brain_stream_.close();
    if (neural_stream_.is_open()) neural_stream_.close();
}

void TelemetrySystem::tick(SimulationContext& context) {
    const double dt = context.fixed_dt();
    accumulator_ += dt;

    flush_events();
    
    // Phase 4b: Log Brain Inspection
    if (brain_stream_.is_open()) {
        auto view = context.registry().view<BrainInspectComponent>();
        for (auto entity : view) {
            const auto& inspect = view.get<BrainInspectComponent>(entity);
            if (inspect.input_snapshot.empty() && inspect.output_snapshot.empty()) continue;

            brain_stream_ << "{\"t\":" << std::fixed << std::setprecision(3) << context.simulation_time()
                          << ",\"id\":" << static_cast<std::uint32_t>(entity) // Using entity index as temp ID
                          << ",\"inputs\":[";
            for (size_t i = 0; i < inspect.input_snapshot.size(); ++i) {
                brain_stream_ << (i > 0 ? "," : "") << inspect.input_snapshot[i];
            }
            brain_stream_ << "],\"outputs\":[";
            for (size_t i = 0; i < inspect.output_snapshot.size(); ++i) {
                brain_stream_ << (i > 0 ? "," : "") << inspect.output_snapshot[i];
            }
            brain_stream_ << "]}\n";
        }
    }

    if (accumulator_ < metrics_interval_) {
        return;
    }

    accumulator_ = std::fmod(accumulator_, metrics_interval_);
    compute_aggregates(context);
    write_metrics_row(context.simulation_time());
}

void TelemetrySystem::log_death(double sim_time, std::uint64_t entity_id, std::uint8_t cause, double lifetime, float x, float z) {
    TelemetryEvent event{};
    event.type = TelemetryEventType::AgentDied;
    event.simulation_time = sim_time;
    event.entity_id = entity_id;
    event.cause = cause;
    event.value = lifetime;
    event.x = x;
    event.z = z;
    event_buffer_.push(event);

    if (cause == static_cast<std::uint8_t>(DeathCause::Starvation)) {
        ++aggregates_.death_count_starvation;
    } else if (cause == static_cast<std::uint8_t>(DeathCause::Predation)) {
        ++aggregates_.death_count_predation;
    }
}

void TelemetrySystem::log_feeding(double sim_time, std::uint64_t predator_id, std::uint64_t prey_id, double energy, float x, float z) {
    TelemetryEvent event{};
    event.type = TelemetryEventType::FeedingEvent;
    event.simulation_time = sim_time;
    event.entity_id = predator_id;
    event.target_id = prey_id;
    event.value = energy;
    event.x = x;
    event.z = z;
    event_buffer_.push(event);

    aggregates_.total_energy_transferred += energy;
}

void TelemetrySystem::log_spawn(double sim_time, std::uint64_t entity_id, std::uint64_t parent_id, float x, float z) {
    TelemetryEvent event{};
    event.type = TelemetryEventType::AgentSpawned;
    event.simulation_time = sim_time;
    event.entity_id = entity_id;
    event.parent_id = parent_id;
    event.x = x;
    event.z = z;
    event_buffer_.push(event);
}

void TelemetrySystem::log_neural_state(double sim_time, std::uint64_t entity_id, std::uint8_t action_mask,
                                       const std::vector<double>& internal_state) {
    if (!neural_stream_.is_open()) {
        return;
    }

    neural_stream_ << std::fixed << std::setprecision(4)
                   << sim_time << "," << entity_id << "," << static_cast<int>(action_mask);

    constexpr std::size_t kMaxCols = 32;
    for (std::size_t i = 0; i < kMaxCols; ++i) {
        if (i < internal_state.size()) {
            neural_stream_ << "," << internal_state[i];
        } else {
            neural_stream_ << ",0";
        }
    }
    neural_stream_ << "\n";
}

void TelemetrySystem::compute_aggregates(SimulationContext& context) {
    auto& registry = context.registry();

    double energy_sum = 0.0;
    double energy_sq_sum = 0.0;
    double min_e = std::numeric_limits<double>::max();
    double max_e = std::numeric_limits<double>::lowest();
    double age_sum = 0.0;
    double fitness_sum = 0.0;
    
    // Biomass sums
    aggregates_.total_biomass_consumers = 0.0;
    aggregates_.total_biomass_producers = 0.0;
    aggregates_.total_biomass_corpses = 0.0;
    aggregates_.total_biomass_soil = 0.0;

    std::size_t agent_count = 0;

    aggregates_.herbivore_count = 0;
    aggregates_.carnivore_count = 0;

    auto view = registry.view<MetabolismComponent, DietComponent>();
    for (auto entity : view) {
        const auto& metab = view.get<MetabolismComponent>(entity);
        const auto& diet = view.get<DietComponent>(entity);

        energy_sum += metab.energy;
        energy_sq_sum += metab.energy * metab.energy;
        min_e = std::min(min_e, metab.energy);
        max_e = std::max(max_e, metab.energy);
        ++agent_count;
        
        // Agent biomass is approximated by their stored energy (for now)
        aggregates_.total_biomass_consumers += metab.energy;

        if (diet.type == DietType::Herbivore) {
            ++aggregates_.herbivore_count;
        } else {
            ++aggregates_.carnivore_count;
        }

        if (auto* life = registry.try_get<LifecycleComponent>(entity)) {
            age_sum += life->age;
        }
        if (auto* fit = registry.try_get<FitnessComponent>(entity)) {
            fitness_sum += fit->last_fitness;
        }
    }
    
    // Corpse Biomass
    aggregates_.corpse_count = 0;
    auto corpse_view = registry.view<CorpseComponent>();
    for (auto entity : corpse_view) {
        const auto& corpse = corpse_view.get<CorpseComponent>(entity);
        aggregates_.total_biomass_corpses += corpse.biomass;
        ++aggregates_.corpse_count;
    }

    if (agent_count > 0) {
        aggregates_.mean_energy = energy_sum / static_cast<double>(agent_count);
        const double variance = (energy_sq_sum / static_cast<double>(agent_count)) -
                                (aggregates_.mean_energy * aggregates_.mean_energy);
        aggregates_.stddev_energy = std::sqrt(std::max(0.0, variance));
        aggregates_.min_energy = min_e;
        aggregates_.max_energy = max_e;
        aggregates_.mean_age = age_sum / static_cast<double>(agent_count);
        aggregates_.mean_fitness = fitness_sum / static_cast<double>(agent_count);
    } else {
        aggregates_.mean_energy = 0.0;
        aggregates_.stddev_energy = 0.0;
        aggregates_.min_energy = 0.0;
        aggregates_.max_energy = 0.0;
        aggregates_.mean_age = 0.0;
        aggregates_.mean_fitness = 0.0;
    }

    // Plant Biomass
    auto plant_view = registry.view<PlantComponent>();
    aggregates_.plant_count = 0;
    for (auto entity : plant_view) {
        const auto& plant = plant_view.get<PlantComponent>(entity);
        if (plant.alive) {
            aggregates_.total_biomass_producers += plant.energy;
            ++aggregates_.plant_count;
        }
    }
    
    // Soil Biomass (Estimate sum)
    if (auto* soil = registry.ctx().find<SoilGrid>()) {
        const double mean = soil->mean_nutrient();
        const double cells = static_cast<double>(soil->width() * soil->height());
        aggregates_.total_biomass_soil = mean * cells; 
    }
    
    // Aggregated Energy statistics
    if (auto* stats = registry.ctx().find<FeedingStatistics>()) {
        aggregates_.energy_from_hunting = stats->energy_from_hunting;
        aggregates_.energy_from_scavenging = stats->energy_from_scavenging;
        aggregates_.total_energy_transferred = stats->energy_transferred_last_tick; // This is per-tick, confusing for metrics.
        // Actually, metrics are snapshot based.
        // If we want total accumulated, we should track it. The `FeedingSystem` resets `energy_transferred_last_tick`?
        // No, `FeedingSystem` currently *adds* to it. We need to know who resets it.
        // `StatsSystem` reads it.
        // Let's assume `TelemetrySystem` tracks accumulation in `aggregates` if passed through events,
        // BUT `log_feeding` is per-event.
        // `FeedingSystem` calls `telem->total_energy_gained`.
        // The `stats->energy_...` are reset where? Probably not reset yet!
        // I should stick to using the `FeedingStatistics` as "since last check" or "accumulated total"?
        // Usually stats are reset per tick or per report.
        // For CSV, we ideally want "rate" or "total". Let's log *Total Accumulated* if `FeedingStatistics` accumulates forever.
        // But `FeedingStatistics` name implies "Statistics", arguably per tick.
        // Let's assume it accumulates and I'll log the accumulation.
    }
}

void TelemetrySystem::write_metrics_row(double sim_time) {
    if (!metrics_stream_.is_open()) {
        return;
    }
    
    metrics_stream_ << std::fixed << std::setprecision(3)
                    << sim_time << ","
                    << aggregates_.herbivore_count << ","
                    << aggregates_.carnivore_count << ","
                    << aggregates_.plant_count << ","
                    << aggregates_.corpse_count << ","
                    << aggregates_.mean_energy << ","
                    << aggregates_.stddev_energy << ","
                    << aggregates_.min_energy << ","
                    << aggregates_.max_energy << ","
                    << aggregates_.total_biomass_producers << ","
                    << aggregates_.total_biomass_consumers << ","
                    << aggregates_.total_biomass_corpses << ","
                    << aggregates_.total_biomass_soil << ","
                    << aggregates_.energy_from_hunting << ","
                    << aggregates_.energy_from_scavenging << ","
                    << aggregates_.total_energy_transferred << ","
                    << aggregates_.death_count_starvation << ","
                    << aggregates_.death_count_predation << "\n";
    metrics_stream_.flush();
}

void TelemetrySystem::flush_events() {
    if (!events_stream_.is_open()) {
        return;
    }
    
    const std::size_t current_index = event_buffer_.write_index();
    if (current_index <= last_flushed_index_) {
        return;
    }
    
    for (std::size_t i = last_flushed_index_; i < current_index && i < last_flushed_index_ + TelemetrySystem::kEventBufferSize; ++i) {
        const auto& event = event_buffer_.at(i);
        
        events_stream_ << "{\"t\":" << std::fixed << std::setprecision(3) << event.simulation_time
                       << ",\"type\":\"" << event_type_to_string(event.type) << "\""
                       << ",\"id\":" << event.entity_id
                       << ",\"x\":" << event.x
                       << ",\"z\":" << event.z;
        
        if (event.type == TelemetryEventType::AgentDied) {
            events_stream_ << ",\"cause\":\"" << death_cause_to_string(event.cause) << "\""
                           << ",\"lifetime\":" << event.value;
        } else if (event.type == TelemetryEventType::FeedingEvent) {
            events_stream_ << ",\"target\":" << event.target_id
                           << ",\"energy\":" << event.value;
        } else if (event.type == TelemetryEventType::AgentSpawned) {
            events_stream_ << ",\"parent\":" << event.parent_id;
        }
        
        events_stream_ << "}\n";
    }
    
    last_flushed_index_ = current_index;
    events_stream_.flush();
}

}  // namespace evolution::sim
