#include "evolution/sim/telemetry_system.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"

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
    
    if (metrics_stream_.is_open()) {
        metrics_stream_ << "time,herbivores,carnivores,plants,mean_energy,stddev_energy,"
                        << "min_energy,max_energy,mean_age,mean_fitness,"
                        << "deaths_starvation,deaths_predation,energy_transferred\n";
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
}

TelemetrySystem::~TelemetrySystem() {
    flush_events();
    if (metrics_stream_.is_open()) {
        metrics_stream_.close();
    }
    if (events_stream_.is_open()) {
        events_stream_.close();
    }
}

void TelemetrySystem::tick(SimulationContext& context) {
    const double dt = context.fixed_dt();
    accumulator_ += dt;

    flush_events();

    if (accumulator_ < metrics_interval_) {
        return;
    }

    accumulator_ = std::fmod(accumulator_, metrics_interval_);
    compute_aggregates(context);
    write_metrics_row(context.simulation_time());
}

void TelemetrySystem::log_death(double sim_time, std::uint64_t entity_id, std::uint8_t cause, double lifetime) {
    TelemetryEvent event{};
    event.type = TelemetryEventType::AgentDied;
    event.simulation_time = sim_time;
    event.entity_id = entity_id;
    event.cause = cause;
    event.value = lifetime;
    event_buffer_.push(event);

    if (cause == static_cast<std::uint8_t>(DeathCause::Starvation)) {
        ++aggregates_.death_count_starvation;
    } else if (cause == static_cast<std::uint8_t>(DeathCause::Predation)) {
        ++aggregates_.death_count_predation;
    }
}

void TelemetrySystem::log_feeding(double sim_time, std::uint64_t predator_id, std::uint64_t prey_id, double energy) {
    TelemetryEvent event{};
    event.type = TelemetryEventType::FeedingEvent;
    event.simulation_time = sim_time;
    event.entity_id = predator_id;
    event.target_id = prey_id;
    event.value = energy;
    event_buffer_.push(event);

    aggregates_.total_energy_transferred += energy;
}

void TelemetrySystem::log_spawn(double sim_time, std::uint64_t entity_id) {
    TelemetryEvent event{};
    event.type = TelemetryEventType::AgentSpawned;
    event.simulation_time = sim_time;
    event.entity_id = entity_id;
    event_buffer_.push(event);
}

void TelemetrySystem::compute_aggregates(SimulationContext& context) {
    auto& registry = context.registry();

    double energy_sum = 0.0;
    double energy_sq_sum = 0.0;
    double min_e = std::numeric_limits<double>::max();
    double max_e = std::numeric_limits<double>::lowest();
    double age_sum = 0.0;
    double fitness_sum = 0.0;
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

    auto plant_view = registry.view<PlantComponent>();
    aggregates_.plant_count = 0;
    for (auto entity : plant_view) {
        const auto& plant = plant_view.get<PlantComponent>(entity);
        if (plant.alive) {
            ++aggregates_.plant_count;
        }
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
                    << aggregates_.mean_energy << ","
                    << aggregates_.stddev_energy << ","
                    << aggregates_.min_energy << ","
                    << aggregates_.max_energy << ","
                    << aggregates_.mean_age << ","
                    << aggregates_.mean_fitness << ","
                    << aggregates_.death_count_starvation << ","
                    << aggregates_.death_count_predation << ","
                    << aggregates_.total_energy_transferred << "\n";
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
                       << ",\"id\":" << event.entity_id;
        
        if (event.type == TelemetryEventType::AgentDied) {
            events_stream_ << ",\"cause\":\"" << death_cause_to_string(event.cause) << "\""
                           << ",\"lifetime\":" << event.value;
        } else if (event.type == TelemetryEventType::FeedingEvent) {
            events_stream_ << ",\"target\":" << event.target_id
                           << ",\"energy\":" << event.value;
        }
        
        events_stream_ << "}\n";
    }
    
    last_flushed_index_ = current_index;
    events_stream_.flush();
}

}  // namespace evolution::sim
