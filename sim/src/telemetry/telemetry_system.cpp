#include "evolution/sim/telemetry_system.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/population_monitor.h"

namespace evolution::sim {

namespace {

constexpr std::string_view kEventTypeToString[] = {
    "SPECIES_CREATED",
    "SPECIES_EXTINCT",
    "ENTITY_SPAWN",
    "ENTITY_DEATH",
    "FEEDING_EVENT",
    "BRAIN_OUTPUT",
    "ACTUATION_APPLIED",
    "MOVEMENT_METRIC",
    "GENOME_TRAITS",
    "LINEAGE_LINK",
    "ROLLUP_SNAPSHOT",
    "POPULATION_RESCUE",
    "OVERPOPULATION_CULL",
};

static_assert(sizeof(kEventTypeToString) / sizeof(kEventTypeToString[0]) ==
              static_cast<std::size_t>(TelemetryEventType::OVERPOPULATION_CULL) + 1,
              "Event type string array must match enum");

constexpr std::string_view kDeathCauseToString[] = {
    "STARVATION",
    "OLD_AGE",
    "UNKNOWN",
};

static_assert(sizeof(kDeathCauseToString) / sizeof(kDeathCauseToString[0]) ==
              static_cast<std::size_t>(DeathCause::UNKNOWN) + 1,
              "Death cause string array must match enum");

std::string EscapeJsonString(const std::string& s) {
    std::ostringstream oss;
    for (const char c : s) {
        const unsigned char uc = static_cast<unsigned char>(c);
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (uc < 0x20) {
                    oss << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                        << static_cast<int>(uc);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

[[nodiscard]] bool IsMissingOrEmptyFile(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        return false;
    }
    if (!exists) {
        return true;
    }

    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return false;
    }
    return size == 0;
}

}  // namespace

TelemetrySystem::TelemetrySystem(const std::filesystem::path& output_dir,
                                 std::string_view run_id,
                                 const TelemetryTargeting& targeting,
                                 const RollupConfig& rollup_config)
    : output_dir_(output_dir),
      run_id_(run_id),
      targeting_(targeting),
      rollup_config_(rollup_config) {
    const auto telemetry_dir = output_dir_ / "telemetry";
    std::filesystem::create_directories(telemetry_dir);
    event_buffer_.reserve(rollup_config_.buffer_size);

    spdlog::info("[TelemetrySystem] Initialized: output_dir={}, run_id={}, sampling_rate={}",
                 output_dir_.string(), run_id_, targeting_.sampling_rate);
}

TelemetrySystem::~TelemetrySystem() {
    try {
        flush();
    } catch (const std::exception& e) {
        spdlog::error("[TelemetrySystem] Failed to flush on destruction: {}", e.what());
    }
}

void TelemetrySystem::tick(SimulationContext& context) {
    const double dt = context.fixed_dt();
    ++tick_counter_;
    events_window_accumulator_ += dt;
    if (events_window_accumulator_ >= 1.0) {
        events_window_accumulator_ = std::fmod(events_window_accumulator_, 1.0);
        events_window_total_ = 0;
        events_window_by_type_.clear();
    }

    if (rollup_config_.interval_seconds > 0.0) {
        rollup_accumulator_ += dt;
        if (rollup_accumulator_ >= rollup_config_.interval_seconds) {
            EmitRollup(context);
            rollup_accumulator_ = 0.0;
        }
    }

    const bool has_targeted_capture = !targeting_.target_species.empty() ||
                                      !targeting_.target_entities.empty() ||
                                      !targeting_.target_lineages.empty();
    const bool should_emit_movement =
        (rollup_config_.movement_capture_mode == MovementCaptureMode::Sampled &&
         (targeting_.sampling_rate > 0.0 || has_targeted_capture)) ||
        (rollup_config_.movement_capture_mode == MovementCaptureMode::TargetedOnly && has_targeted_capture);
    if (should_emit_movement) {
        EmitMovementMetrics(context);
    }

    if (event_buffer_.size() >= rollup_config_.buffer_size) {
        flush();
    }
}

bool TelemetrySystem::emit_event(const TelemetryEvent& event, bool force_capture) {
    ++events_attempted_total_;

    if (!force_capture && !should_sample()) {
        ++events_dropped_total_;
        return false;
    }

    if (!force_capture) {
        if (rollup_config_.max_events_per_second > 0 &&
            events_window_total_ >= rollup_config_.max_events_per_second) {
            ++events_dropped_total_;
            return false;
        }
        const auto by_type = events_window_by_type_.find(event.type);
        const std::size_t count_for_type = by_type != events_window_by_type_.end() ? by_type->second : 0;
        if (rollup_config_.max_events_per_type_per_second > 0 &&
            count_for_type >= rollup_config_.max_events_per_type_per_second) {
            ++events_dropped_total_;
            return false;
        }
    }

    ++events_window_total_;
    ++events_window_by_type_[event.type];
    ++events_written_total_;

    event_buffer_.push_back(event);
    return true;
}

void TelemetrySystem::flush() {
    if (event_buffer_.empty()) {
        return;
    }

    const std::size_t flush_count = event_buffer_.size();
    for (const auto& event : event_buffer_) {
        WriteEventJsonl(event);
    }

    event_buffer_.clear();
    spdlog::debug("[TelemetrySystem] Flushed {} events", flush_count);
}

bool TelemetrySystem::should_capture(entt::entity entity,
                                     SpeciesId species_id,
                                     genetics::GenomeId genome_id) const noexcept {
    return ShouldCaptureTargeted(entity, species_id, genome_id);
}

bool TelemetrySystem::should_sample() const noexcept {
    if (targeting_.sampling_rate <= 0.0) {
        return false;
    }
    if (targeting_.sampling_rate >= 1.0) {
        return true;
    }
    const double clamped_rate = std::clamp(targeting_.sampling_rate, 0.0, 1.0);
    const auto interval = static_cast<std::uint64_t>(std::ceil(1.0 / clamped_rate));
    return interval > 0 && (tick_counter_ % interval == 0);
}

void TelemetrySystem::EmitRollup(SimulationContext& context) {
    auto& registry = context.registry();
    GlobalRollup rollup;
    rollup.sim_time = context.simulation_time();
    rollup.total_population = registry.storage<entt::entity>().in_use();

    double energy_sum = 0.0;
    std::size_t metabolism_count = 0;
    std::unordered_map<SpeciesId, std::pair<std::size_t, double>> species_stats;

    auto metabolism_view = registry.view<MetabolismComponent>();
    metabolism_view.each([&](const MetabolismComponent& metabolism) {
        energy_sum += metabolism.energy;
        ++metabolism_count;
    });

    auto species_view = registry.view<MetabolismComponent, GenomeHandleComponent>();
    species_view.each([&](entt::entity,
                          const MetabolismComponent& metabolism,
                          const GenomeHandleComponent& genome) {
        // SpeciesIndexContext isn't available in this branch; bucket by genome id
        // to keep rollups populated without adding new context wiring.
        const auto species_id = static_cast<SpeciesId>(genome.id);
        auto& stats = species_stats[species_id];
        ++stats.first;
        stats.second += metabolism.energy;
    });

    rollup.mean_energy = metabolism_count > 0 ? energy_sum / static_cast<double>(metabolism_count) : 0.0;
    rollup.species_count = species_stats.size();

    if (auto* stats = registry.ctx().find<FeedingStatistics>()) {
        rollup.total_feeding_energy = stats->energy_transferred_last_tick;
    }
    if (const auto* monitor = registry.ctx().find<PopulationMonitor>()) {
        rollup.creature_density = monitor->latest.creature_density;
        rollup.herbivore_to_plant_ratio = monitor->latest.herbivore_to_plant_ratio;
        rollup.carnivore_to_herbivore_ratio = monitor->latest.carnivore_to_herbivore_ratio;
        rollup.population_stability_index = monitor->latest.population_stability_index;
    }
    if (const auto* counters = registry.ctx().find<PopulationEventCounters>()) {
        const double dt_window = std::max(1e-6, rollup.sim_time - prev_rollup_time_);
        const std::uint64_t births_delta = counters->births_total - prev_births_total_;
        const std::uint64_t deaths_delta = counters->deaths_total - prev_deaths_total_;
        rollup.reproduction_rate = static_cast<double>(births_delta) / dt_window;
        rollup.death_rate = static_cast<double>(deaths_delta) / dt_window;
        rollup.rescue_count = counters->rescues_total;
        rollup.cull_count = counters->culls_total;
        prev_births_total_ = counters->births_total;
        prev_deaths_total_ = counters->deaths_total;
    }
    rollup.events_dropped = events_dropped_total_;
    rollup.events_written = events_written_total_;
    rollup.effective_sampling_rate =
        events_attempted_total_ > 0
            ? static_cast<double>(events_written_total_) / static_cast<double>(events_attempted_total_)
            : 1.0;
    prev_rollup_time_ = rollup.sim_time;

    rollup.species_rollups.reserve(species_stats.size());
    for (const auto& [species_id, stats] : species_stats) {
        SpeciesRollup species_rollup;
        species_rollup.species_id = species_id;
        species_rollup.sim_time = rollup.sim_time;
        species_rollup.population = stats.first;
        species_rollup.mean_energy = stats.first > 0 ? stats.second / static_cast<double>(stats.first) : 0.0;
        rollup.species_rollups.push_back(std::move(species_rollup));
    }

    WriteRollupCsv(rollup);
    WriteSpeciesRollupCsv(rollup);
}

void TelemetrySystem::EmitMovementMetrics(SimulationContext& context) {
    auto& registry = context.registry();
    auto view = registry.view<TransformComponent, KinematicsComponent>();

    std::unordered_set<entt::entity> seen;
    seen.reserve(view.size_hint());

    for (auto entity : view) {
        seen.insert(entity);
        const auto& transform = view.get<TransformComponent>(entity);
        const auto& kinematics = view.get<KinematicsComponent>(entity);
        const auto* genome = registry.try_get<GenomeHandleComponent>(entity);
        const genetics::GenomeId genome_id = genome != nullptr ? genome->id : 0;

        const bool force_capture = should_capture(entity, 0, genome_id);
        const bool sampled = force_capture || should_sample();

        auto it = last_positions_.find(entity);
        if (it != last_positions_.end() && sampled) {
            const Vec3 delta = transform.position - it->second;
            const double distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

            std::ostringstream payload;
            payload << "{"
                    << "\"entity_id\":" << static_cast<std::uint32_t>(entity)
                    << ",\"genome_id\":" << genome_id
                    << ",\"dx\":" << delta.x
                    << ",\"dy\":" << delta.y
                    << ",\"dz\":" << delta.z
                    << ",\"distance\":" << distance
                    << ",\"vx\":" << kinematics.linear_velocity.x
                    << ",\"vy\":" << kinematics.linear_velocity.y
                    << ",\"vz\":" << kinematics.linear_velocity.z
                    << "}";

            TelemetryEvent event{
                TelemetryEventType::MOVEMENT_METRIC,
                context.simulation_time(),
                payload.str()
            };
            emit_event(event, force_capture);
        }

        last_positions_[entity] = transform.position;
    }

    for (auto it = last_positions_.begin(); it != last_positions_.end();) {
        if (seen.find(it->first) == seen.end()) {
            it = last_positions_.erase(it);
        } else {
            ++it;
        }
    }
}

void TelemetrySystem::WriteEventJsonl(const TelemetryEvent& event) {
    const auto events_file = output_dir_ / std::string(kEventsFile);
    std::ofstream out(events_file, std::ios::app);
    if (!out) {
        throw std::runtime_error("Failed to open events file for writing: " + events_file.string());
    }

    out << "{"
        << "\"schema_version\":" << TELEMETRY_SCHEMA_VERSION
        << ",\"run_id\":\"" << EscapeJsonString(run_id_) << "\""
        << ",\"type\":\"" << kEventTypeToString[static_cast<std::size_t>(event.type)] << "\""
        << ",\"sim_time\":" << event.sim_time
        << ",\"payload\":" << event.payload
        << "}\n";
}

void TelemetrySystem::WriteRollupCsv(const GlobalRollup& rollup) {
    const auto rollups_file = output_dir_ / std::string(kRollupsFile);
    const bool should_write_header = !csv_header_written_ && IsMissingOrEmptyFile(rollups_file);

    std::ofstream out(rollups_file, std::ios::app);
    if (!out) {
        throw std::runtime_error("Failed to open rollups file for writing: " + rollups_file.string());
    }

    if (should_write_header) {
        out << "schema_version,run_id,sim_time,total_population,mean_energy,total_feeding_energy,creature_density,herbivore_to_plant_ratio,carnivore_to_herbivore_ratio,reproduction_rate,death_rate,population_stability_index,rescue_count,cull_count,events_dropped,events_written,effective_sampling_rate\n";
    }

    csv_header_written_ = true;

    out << TELEMETRY_SCHEMA_VERSION << ","
        << EscapeJsonString(run_id_) << ","
        << rollup.sim_time << ","
        << rollup.total_population << ","
        << rollup.mean_energy << ","
        << rollup.total_feeding_energy << ","
        << rollup.creature_density << ","
        << rollup.herbivore_to_plant_ratio << ","
        << rollup.carnivore_to_herbivore_ratio << ","
        << rollup.reproduction_rate << ","
        << rollup.death_rate << ","
        << rollup.population_stability_index << ","
        << rollup.rescue_count << ","
        << rollup.cull_count << ","
        << rollup.events_dropped << ","
        << rollup.events_written << ","
        << rollup.effective_sampling_rate << "\n";
}

void TelemetrySystem::WriteSpeciesRollupCsv(const GlobalRollup& rollup) {
    if (rollup.species_rollups.empty()) {
        return;
    }

    const auto rollups_file = output_dir_ / std::string(kSpeciesRollupsFile);
    const bool should_write_header = !species_csv_header_written_ && IsMissingOrEmptyFile(rollups_file);

    std::ofstream out(rollups_file, std::ios::app);
    if (!out) {
        throw std::runtime_error("Failed to open species rollups file for writing: " + rollups_file.string());
    }

    if (should_write_header) {
        out << "schema_version,run_id,sim_time,species_id,population,mean_energy\n";
    }

    species_csv_header_written_ = true;

    for (const auto& species_rollup : rollup.species_rollups) {
        out << TELEMETRY_SCHEMA_VERSION << ","
            << EscapeJsonString(run_id_) << ","
            << species_rollup.sim_time << ","
            << species_rollup.species_id << ","
            << species_rollup.population << ","
            << species_rollup.mean_energy << "\n";
    }
}

bool TelemetrySystem::ShouldCaptureTargeted(entt::entity entity,
                                           SpeciesId species_id,
                                           genetics::GenomeId genome_id) const noexcept {
    if (!targeting_.target_entities.empty()) {
        return targeting_.target_entities.contains(entity);
    }
    if (!targeting_.target_species.empty() && species_id != 0) {
        return targeting_.target_species.contains(species_id);
    }
    if (!targeting_.target_lineages.empty() && genome_id != 0) {
        return targeting_.target_lineages.contains(genome_id);
    }
    return false;
}

}  // namespace evolution::sim
