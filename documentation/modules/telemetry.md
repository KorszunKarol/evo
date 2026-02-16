## Overview

The telemetry module captures event-based and aggregated simulation data for offline analysis. It writes JSONL event streams and CSV rollups to disk, enabling downstream ingestion into Parquet and analysis via notebooks.

## Files

- `sim/include/evolution/sim/telemetry_system.h`
- `sim/src/telemetry/telemetry_system.cpp`

## Classes

### TelemetrySystem

**Purpose**: Capture telemetry events and periodic rollups for analytics.

**Responsibilities**:
- Buffer and flush JSONL event records
- Emit periodic CSV rollups
- Emit movement metrics for entities
- Apply targeting and sampling controls
- Expose a registry context for other systems

**Public API**:

```cpp
explicit TelemetrySystem(const std::filesystem::path& output_dir,
                         std::string_view run_id = "default",
                         const TelemetryTargeting& targeting = {},
                         const RollupConfig& rollup_config = {});

void tick(SimulationContext& context);
bool emit_event(const TelemetryEvent& event, bool force_capture = false);
void flush();
bool should_capture(entt::entity entity, SpeciesId species_id = 0, genetics::GenomeId genome_id = 0) const noexcept;
bool should_sample() const noexcept;
void set_targeting(const TelemetryTargeting& targeting) noexcept;
```

**Outputs**:
- `telemetry/events.jsonl`: Event stream with schema version, run id, event type, sim time, payload
- `telemetry/metrics.csv`: Global rollup snapshots per interval
- `telemetry/species_rollups.csv`: Per-species rollup snapshots

**Event Types**:
- `SPECIES_CREATED`, `SPECIES_EXTINCT`
- `ENTITY_SPAWN`, `ENTITY_DEATH`
- `FEEDING_EVENT`
- `BRAIN_OUTPUT`, `ACTUATION_APPLIED`
- `MOVEMENT_METRIC`
- `GENOME_TRAITS`
- `LINEAGE_LINK`

**Thread Safety**:
- Not thread-safe; must run on simulation thread

## Data Contracts

### Telemetry Context Access

- Telemetry system is exposed via `entt::registry::ctx()` as `TelemetryContext`
- Systems should check for context existence before emitting events

### Event Schema

Each JSONL record contains:

```json
{
  "schema_version": 2,
  "run_id": "default",
  "type": "ENTITY_SPAWN",
  "sim_time": 12.34,
  "payload": { "entity_id": 42 }
}
```

### Rollup Schema

CSV columns:

```
schema_version,run_id,sim_time,total_population,mean_energy,total_feeding_energy
```

Per-species rollups:

```
schema_version,run_id,sim_time,species_id,population,mean_energy
```
