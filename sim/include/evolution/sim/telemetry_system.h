/**
 * @file telemetry_system.h
 * @brief Event-based telemetry system for offline analytics.
 */

#pragma once

#include <stdint.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <entt/entt.hpp>

#include "evolution/genetics/genome_types.h"
#include "evolution/sim/math_types.h"
#include "evolution/sim/scheduler.h"
#include "evolution/sim/species_index_system.h"

namespace evolution::sim {

/// @brief Current schema version for telemetry outputs.
constexpr std::uint32_t TELEMETRY_SCHEMA_VERSION = 2;

/// @brief Supported telemetry event types.
enum class TelemetryEventType : uint8_t {
    SPECIES_CREATED,
    SPECIES_EXTINCT,
    ENTITY_SPAWN,
    ENTITY_DEATH,
    FEEDING_EVENT,
    BRAIN_OUTPUT,
    ACTUATION_APPLIED,
    MOVEMENT_METRIC,
    GENOME_TRAITS,
    LINEAGE_LINK,
    ROLLUP_SNAPSHOT,
};

/// @brief Death causes for ENTITY_DEATH events.
enum class DeathCause : uint8_t {
    STARVATION,
    OLD_AGE,
    UNKNOWN,
};

/// @brief Selective telemetry capture configuration.
struct TelemetryTargeting {
    std::unordered_set<SpeciesId> target_species{};
    std::unordered_set<entt::entity> target_entities{};
    std::unordered_set<genetics::GenomeId> target_lineages{};
    double sampling_rate{0.0};
};

/// @brief Rollup aggregation configuration.
struct RollupConfig {
    double interval_seconds{1.0};
    std::size_t buffer_size{1000};
};

/// @brief Telemetry event payload wrapper.
struct TelemetryEvent {
    TelemetryEventType type{TelemetryEventType::ENTITY_SPAWN};
    double sim_time{0.0};
    std::string payload{};
};

/// @brief Per-species rollup metrics.
struct SpeciesRollup {
    SpeciesId species_id{0};
    double sim_time{0.0};
    std::size_t population{0};
    double mean_energy{0.0};
    std::unordered_map<DeathCause, std::size_t> deaths_by_cause{};
    std::unordered_map<uint8_t, double> feeding_energy_by_source{};
};

/// @brief Global rollup metrics.
struct GlobalRollup {
    double sim_time{0.0};
    std::size_t total_population{0};
    std::size_t species_count{0};
    double mean_energy{0.0};
    double total_feeding_energy{0.0};
    std::vector<SpeciesRollup> species_rollups{};
};

/// @brief Event-based telemetry system for offline analytics.
class TelemetrySystem final : public ISystem {
public:
    explicit TelemetrySystem(const std::filesystem::path& output_dir,
                             std::string_view run_id = "default",
                             const TelemetryTargeting& targeting = {},
                             const RollupConfig& rollup_config = {});

    ~TelemetrySystem() override;

    void tick(SimulationContext& context) override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    bool emit_event(const TelemetryEvent& event, bool force_capture = false);

    void flush();

    [[nodiscard]] bool should_capture(entt::entity entity,
                                       SpeciesId species_id = 0,
                                       genetics::GenomeId genome_id = 0) const noexcept;

    [[nodiscard]] bool should_sample() const noexcept;

    [[nodiscard]] const TelemetryTargeting& targeting() const noexcept { return targeting_; }

    void set_targeting(const TelemetryTargeting& targeting) noexcept { targeting_ = targeting; }

private:
    void EmitRollup(SimulationContext& context);
    void EmitMovementMetrics(SimulationContext& context);
    void WriteEventJsonl(const TelemetryEvent& event);
    void WriteRollupCsv(const GlobalRollup& rollup);
    void WriteSpeciesRollupCsv(const GlobalRollup& rollup);
    bool ShouldCaptureTargeted(entt::entity entity,
                               SpeciesId species_id,
                               genetics::GenomeId genome_id) const noexcept;

    static constexpr std::string_view name_ = "telemetry";
    static constexpr std::string_view kEventsFile = "telemetry/events.jsonl";
    static constexpr std::string_view kRollupsFile = "telemetry/metrics.csv";
    static constexpr std::string_view kSpeciesRollupsFile = "telemetry/species_rollups.csv";

    std::filesystem::path output_dir_;
    std::string run_id_;
    TelemetryTargeting targeting_;
    RollupConfig rollup_config_;

    double rollup_accumulator_{0.0};
    std::vector<TelemetryEvent> event_buffer_{};
    bool csv_header_written_{false};
    bool species_csv_header_written_{false};
    uint64_t tick_counter_{0};
    std::unordered_map<entt::entity, Vec3> last_positions_{};
};

/// @brief Registry context wrapper providing access to the telemetry system.
struct TelemetryContext {
    TelemetrySystem* system{nullptr};
};

}  // namespace evolution::sim
