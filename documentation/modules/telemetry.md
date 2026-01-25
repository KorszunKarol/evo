# Module: Telemetry

## Overview

The Telemetry module provides structured logging, metrics collection, and analysis tools for the evolution simulation. It tracks energy flows, population dynamics, evolutionary statistics, and system performance metrics.

## Files

- `/home/karolito/evolution/sim/include/evolution/sim/telemetry_system.h`
- `/home/karolito/evolution/sim/src/systems/telemetry_system.cpp`

## Component Definitions

### TelemetryComponent

**Purpose**: Per-entity metrics tracking for detailed agent-level analytics.

**Fields**:
- `energy_gained` (double) - Total energy acquired (feeding, reproduction rewards, etc.)
- `energy_lost_metabolism` (double) - Total energy consumed by basal metabolism
- `energy_lost_movement` (double) - Total energy spent on locomotion
- `distance_traveled` (double) - Total world-space distance traveled
- `successful_feeds` (uint32_t) - Count of successful feeding events
- `kill_count` (uint32_t) - Number of prey killed (for predators)
- `death_cause` (DeathCause) - Cause of death (if dead)
- `killed_by_predation` (uint32_t) - Count of times killed by predators (for prey)

**Data Contract**:
- **Readers**: TelemetrySystem, EvolutionSystem (fitness evaluation)
- **Writers**: Multiple systems (MetabolismSystem, MotorSystem, FeedingSystem, DecompositionSystem)
- **Lifetime**: Persists while entity alive; snapshot before death

### BrainInspectComponent

**Purpose**: Debug/inspection snapshots for brain behavior analysis.

**Fields**:
- `input_snapshot` (std::vector<double>) - Brain inputs at capture time
- `output_snapshot` (std::vector<double>) - Brain outputs at capture time
- `internal_state` (std::vector<double>) - Internal hidden layer activations (for MLP)
- `gating_snapshot` (std::vector<double>) - Gating unit states (if using gated architectures)
- `action_mask` (uint32_t) - Bitmask of which outputs were active

**Data Contract**:
- **Readers**: BrainInferenceSystem (populates), TelemetrySystem (logs)
- **Writers**: BrainInferenceSystem (capture intervals)
- **Lifetime**: Updated at configured inspection intervals

## Systems

### TelemetrySystem

**Purpose**: Aggregates metrics across all entities, writes logs, updates statistics.

**Responsibilities**:

#### 1. Energy Flow Tracking

```cpp
for (auto entity : registry.view<TelemetryComponent>()) {
    auto& telemetry = registry.get<TelemetryComponent>(entity);

    // Log energy sources/sinks
    log_event("EnergyIn", {
        "entity": entity,
        "source": telemetry.energy_gained - previous_gain,
        "total": telemetry.energy_gained
    });

    log_event("EnergyOut", {
        "entity": entity,
        "metabolism": telemetry.energy_lost_metabolism,
        "movement": telemetry.energy_lost_movement,
        "total": telemetry.energy_lost_metabolism + telemetry.energy_lost_movement
    });
}
```

#### 2. Population Dynamics

```cpp
// Count by species
std::map<SpeciesId, uint32_t> population_counts;

for (auto entity : registry.view<GenomeHandleComponent>()) {
    auto& genome_handle = registry.get<GenomeHandleComponent>(entity);
    SpeciesId species = classify_species(genome_handle.id);

    population_counts[species]++;
}

log_event("Population", {
    "timestamp": sim_time,
    "by_species": population_counts,
    "total_population": registry.alive()
});
```

#### 3. Evolutionary Statistics

```cpp
for (auto entity : registry.view<FitnessComponent>()) {
    auto& fitness = registry.get<FitnessComponent>(entity);
    auto& genome_handle = registry.get<GenomeHandleComponent>(entity);

    log_event("Fitness", {
        "genome_id": genome_handle.id,
        "generation": genome_handle.generation,
        "age_seconds": fitness.age_seconds,
        "energy_integrated": fitness.energy_int_accum,
        "offspring_count": fitness.offspring_count,
        "fitness": fitness.last_fitness
    });
}
```

#### 4. System Performance

```cpp
for (auto& system : scheduler.systems()) {
    log_event("SystemPerf", {
        "system": system.name(),
        "execution_time_ms": system.last_execution_time_ms,
        "entity_count": system.entity_count_processed,
        "tick": current_tick
    });
}
```

## Log Format

### Structured Logging

All telemetry events use consistent JSON-like structure:

```json
{
  "event_type": "EnergyFlow",
  "timestamp": 1234.567,
  "tick": 12345,
  "data": {
    "entity": 42,
    "energy_gained": 15.3,
    "energy_lost_metabolism": 8.2,
    "energy_lost_movement": 2.1,
    "net_change": 5.0
  }
}
```

### Log Levels

- **INFO**: Population counts, energy summaries (emitted every N ticks)
- **DEBUG**: Per-entity detailed events (if debug mode enabled)
- **WARN**: Unusual events (starvation spikes, energy depletion warnings)
- **ERROR**: System failures, invalid states

## Metrics and Analysis

### Population-Level Metrics

**Energy Budget**:
- Total ecosystem energy = Σ(all entity energy)
- Energy flow rate = Δ(total energy) / Δ(time)
- Energy efficiency = (biomass produced) / (energy consumed)

**Species Diversity**:
- Shannon entropy = -Σ(p_i × log2(p_i)) where p_i = proportion of species i
- Species richness = count of species with N > threshold
- Simpson diversity = 1 / Σ(p_i²)

**Age Structure**:
- Age distribution histogram (bins: 0-10s, 10-100s, 100-1000s, >1000s)
- Mean age = Σ(age × N) / Σ(N)
- Median age = 50th percentile

### Evolutionary Metrics

**Fitness Distribution**:
- Fitness mean, median, std deviation
- Fitness by generation (trend analysis)
- Best fitness by genome ID

**Trait Evolution**:
- Track trait changes over generations
- Trait covariance matrix (correlations)
- Trait diversity measures

### System Performance

**Execution Time**:
- Per-system mean/p95/p99 latency
- Bottleneck identification (slowest systems)
- Tick time budgeting

**Memory Usage**:
- Registry memory footprint
- Spatial index memory
- Genome storage size

## Configuration

### TelemetryConfig

```cpp
struct TelemetryConfig {
    double log_interval; // Seconds between aggregate logs (default: 1.0)
    bool per_entity_logging; // Enable detailed per-entity events (default: false)
    bool system_performance; // Log system execution times (default: true)
    std::string log_output_path; // File path (default: "telemetry.jsonl")
};
```

### Log Output

**File Rotation**:
- Rotate logs every N MB or N hours
- Timestamp in filename: `telemetry_YYYYMMDD_HHMMSS.jsonl`

**Log Backends**:
- **File**: Primary backend for analysis
- **Console**: stdout/stderr for live monitoring
- **Network** (Planned): Remote telemetry server

## Analysis Tools

### Post-Processing Pipeline

1. **Parse logs** - Read JSONL files
2. **Aggregate** - Compute time-series metrics
3. **Visualize** - Generate charts (population trends, energy flows)
4. **Analyze** - Identify patterns, bottlenecks, evolutionary pressures

### Key Queries

**Example**: "What caused the most deaths?"
```python
# Parse logs
events = parse_telemetry_logs("telemetry.jsonl")

# Count death causes
death_causes = Counter(e.death_cause for e in events if e.event_type == "Death")

# Plot
bar_chart(death_causes, title="Death Causes")
```

## Brain Inspection

### Capture Triggers

- **Periodic**: Every N ticks (configurable)
- **Event-based**: On unusual events (low energy, combat, etc.)
- **Manual**: Via debug API or keyboard trigger (in client)

### Inspection Data

**Input/Output Snapshots**:
- Full brain state at capture time
- Used for behavior analysis, debugging unexpected actions

**Internal State**:
- Hidden layer activations (MLP)
- NEAT node activations and connection weights
- Used to understand learning dynamics

**Visualization**:
- Render neural network graphs with activations highlighted
- Show input sensor values and output motor commands
- Animate over time (playback of brain behavior)

## Privacy and Data

### Sensitive Data

**Excluded from Logs**:
- No personally identifiable information (entities are numeric IDs)
- No genetic material content (only genome IDs)

**Included in Logs**:
- Entity IDs (required for tracking individuals)
- Genome IDs (required for evolution analysis)
- Behavior patterns (required for scientific analysis)

## Related Documentation

- [Components Module](./components.md) - TelemetryComponent, BrainInspectComponent
- [Evolution Module](./evolution.md) - Fitness tracking
- [Systems Overview](./core_simulation.md) - System execution order
