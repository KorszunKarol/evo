# Draft: Independent Feature Candidates

## Requirements (from user)
- Review `sess` and `session-ses_40a5.md` plus `/documentation/`.
- Propose a feature we can work on independently that does not overlap with the other work.
- Assess whether parallel agents are being used effectively.
- User wants to **totally redesign telemetry/stats/observability** to get detailed simulation insights.
- Consumption goal: **Hybrid (HUD + offline)**.
- Immediate priority: **Deep offline analytics**.
- Storage foundation choice: **Parquet + DuckDB**.
- Not a priority (for now): cross-seed run comparisons.
- Desired insights: deep stats on behavior patterns, inter-species dynamics, and how minds/brains change over time.
- First end-to-end analytics slices: **Species dynamics + Behavior patterns**.
- Data granularity preference: mostly event-based + rollups, but allow opt-in “full logs” for 1 species or N species (selective high-detail capture).
- High-detail targeting controls desired: by species, by entity IDs, and by genome lineage.
- Offline analysis UX: notebooks now; full GUI later.

## Context Observed
- Session `sess` focuses on genetics mutation + evolution generation flow (`sim/src/genetics/*`, `sim/src/systems/evolution_system.cpp`, tests like `tests/sim/test_xor_evolution.cpp`).
- Session `session-ses_40a5.md` focuses on a C++ code quality review workflow (tooling/sanitizers/clang-tidy) and some automated scans.
- Documentation SSOT currently emphasizes environment MVP and telemetry/snapshots: `documentation/requirements.md`.
- Documentation flags missing/underdocumented areas including tick control (pause/resume/step), determinism strategy, and persistence/snapshots: `documentation/undocumented_features.md`.

## Repo Findings (quick scan)
- Telemetry implementation exists and already writes multiple outputs:
  - `sim/src/telemetry/telemetry_system.cpp` (mentions `metrics.csv`, `events.jsonl`, `brain_dump.jsonl`, `brain_activations.csv`).
  - `sim/include/evolution/sim/telemetry_system.h`
  - `sim/include/evolution/sim/components.h` (`TelemetryComponent`)
- There is also a `StatsSystem`:
  - `sim/include/evolution/sim/stats_system.h`
  - `sim/src/systems/stats_system.cpp`
  - Note in `sim/src/core/scenario.cpp`: “StatsSystem removed - TelemetrySystem now handles metrics.”
- Other analytics-style outputs exist:
  - `sim/src/systems/speciation_system.cpp` writes `speciation.csv`
  - `sim/src/systems/trait_analysis_system.cpp` writes `traits.csv`

## Repo Findings (Telemetry code map)
- Telemetry wiring & outputs:
  - `sim/include/evolution/sim/telemetry_system.h` + `sim/src/telemetry/telemetry_system.cpp`
  - Output directory defaults to `telemetry/` and currently writes:
    - `telemetry/metrics.csv`
    - `telemetry/events.jsonl`
    - `telemetry/brain_dump.jsonl`
    - `telemetry/brain_activations.csv`
  - Event producers:
    - `sim/src/environment/feeding_system.cpp` calls telemetry for feeding events
    - `sim/src/systems/metabolism_system.cpp` calls telemetry for death events
    - `sim/src/main.cpp` wires TelemetrySystem into scheduler and attaches telemetry pointers to systems
- Stats/logging hooks:
  - `sim/src/systems/stats_system.cpp` emits periodic aggregate logs
  - `sim/src/core/scheduler.cpp` traces per-system ticks
  - `sim/src/core/simulation_app.cpp` traces begin/end tick and advances sim time
- FeedingStatistics used by telemetry aggregates:
  - `sim/include/evolution/sim/environment/environment.h` defines `FeedingStatistics`
  - Telemetry aggregates reference these feeding energy flows

## Research Findings (Telemetry docs vs code)
- `documentation/modules/telemetry.md` diverges from implementation in multiple ways:
  - Docs mention a `TelemetryConfig` struct + log rotation + `SystemPerf` events; code does not implement these (uses `TelemetrySystem(double metrics_interval, std::string output_dir)` and fixed output filenames).
  - Event schema mismatch: docs show `{event_type, timestamp, tick, data}`; code writes JSONL with fields like `t`, `type` (DEATH/FEEDING/SPAWN), `id`, `x`, `z`, plus per-event fields.
  - `TelemetryComponent` naming mismatch: docs use `energy_gained`/`energy_lost_*`; code uses `total_energy_gained`/`total_energy_lost_*`.
  - Implemented-but-underdocumented streams: `brain_dump.jsonl` and `brain_activations.csv` exist in code.
  - Aggregates depend on `FeedingStatistics` fields not explicitly covered in docs.

## External Research Findings (observability patterns)
- Recommended split for simulation engines:
  - Aggregate metrics for monitoring + alerting
  - Structured event logs for high-cardinality / per-entity fidelity
- Sampling is essential for per-entity detail at scale (avoid logging every entity @ 60 Hz).
- Storage guidance:
  - JSONL good for streaming events; Parquet good for analytics/columnar queries.
  - Keep schema versioning/backward compatibility as a first-class requirement.
- Time-series rollups: keep high-rate raw data short-lived; create 1-min/1-hr rollups for long-term comparisons.

## Candidate Independent Features (initial)
- Telemetry/stats/observability redesign (now confirmed priority).
- Determinism & replay harness: run-snapshot-compare tool + stable checksums.
- Simulation tick control: pause/resume/single-step in SimulationApp + render client integration.

## Agent/Parallelization Notes
- Parallelism attempt exists (multiple background tasks), but some results were unretrievable (“Task not found”), suggesting tooling misuse or task lifecycle issues.
- One scan output appears partially unreliable (claims not matching code excerpts); needs verification-by-file rather than trusting summaries.

## Open Questions
- Primary consumption target for observability: in-client HUD, offline analysis logs, or metrics scraping?
- Any hard exclusions (e.g., “don’t touch render client”, “no new CLI binary”)?
