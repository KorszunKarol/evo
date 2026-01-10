#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

#include "evolution/sim/scheduler.h"

namespace evolution::sim {

enum class TelemetryEventType : std::uint8_t {
    None = 0,
    AgentDied,
    FeedingEvent,
    ReproductionEvent,
    AgentSpawned
};

struct TelemetryEvent {
    TelemetryEventType type{TelemetryEventType::None};
    double simulation_time{0.0};
    std::uint64_t entity_id{0};
    std::uint64_t target_id{0};
    double value{0.0};
    std::uint8_t cause{0};
};

template <std::size_t Capacity>
class TelemetryRingBuffer {
public:
    void push(const TelemetryEvent& event) noexcept {
        const std::size_t idx = write_index_.fetch_add(1, std::memory_order_relaxed) % Capacity;
        buffer_[idx] = event;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return std::min(write_index_.load(std::memory_order_relaxed), Capacity);
    }

    [[nodiscard]] const TelemetryEvent& at(std::size_t index) const noexcept {
        return buffer_[index % Capacity];
    }

    void clear() noexcept { write_index_.store(0, std::memory_order_relaxed); }

    [[nodiscard]] std::size_t write_index() const noexcept {
        return write_index_.load(std::memory_order_relaxed);
    }

private:
    std::array<TelemetryEvent, Capacity> buffer_{};
    std::atomic<std::size_t> write_index_{0};
};

struct TelemetryAggregates {
    double mean_energy{0.0};
    double stddev_energy{0.0};
    double min_energy{0.0};
    double max_energy{0.0};
    double mean_age{0.0};
    double mean_fitness{0.0};
    std::size_t herbivore_count{0};
    std::size_t carnivore_count{0};
    std::size_t plant_count{0};
    std::size_t death_count_starvation{0};
    std::size_t death_count_predation{0};
    double total_energy_transferred{0.0};
};

/**
 * @brief Multi-channel telemetry system.
 * Streams metrics to CSV, events to JSONL.
 */
class TelemetrySystem final : public ISystem {
public:
    static constexpr std::size_t kEventBufferSize = 4096;

    explicit TelemetrySystem(double metrics_interval = 1.0,
                             const std::string& output_dir = "telemetry") noexcept;
    ~TelemetrySystem() override;

    void tick(SimulationContext& context) override;
    [[nodiscard]] std::string_view name() const noexcept override { return "telemetry"; }

    void log_death(double sim_time, std::uint64_t entity_id, std::uint8_t cause, double lifetime);
    void log_feeding(double sim_time, std::uint64_t predator_id, std::uint64_t prey_id, double energy);
    void log_spawn(double sim_time, std::uint64_t entity_id);

    [[nodiscard]] const TelemetryAggregates& aggregates() const noexcept { return aggregates_; }

private:
    void compute_aggregates(SimulationContext& context);
    void write_metrics_row(double sim_time);
    void flush_events();

    TelemetryRingBuffer<kEventBufferSize> event_buffer_;
    TelemetryAggregates aggregates_;
    double metrics_interval_;
    double accumulator_{0.0};
    std::size_t last_flushed_index_{0};

    std::ofstream metrics_stream_;
    std::ofstream events_stream_;
};

}  // namespace evolution::sim
