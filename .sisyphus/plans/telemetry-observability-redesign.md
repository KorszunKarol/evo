# Telemetry / Stats / Observability Redesign (Offline Analytics First)

## Context

### Original Request
Redesign telemetry/stats/observability to unlock deep, detailed insights into the simulation (species dynamics + behavior patterns, and later mind/brain change), with a future full GUI. For now, prioritize offline analysis via notebooks.

### Interview Summary
- **Primary goal now**: deep offline analytics (notebook-driven), starting with **species dynamics + behavior patterns**.
- **Not a priority yet**: cross-seed/run comparisons.
- **Storage**: target **Parquet + DuckDB**, but **Parquet generation lives in Python notebooks/scripts initially** (sim can keep writing JSONL/CSV).
- **Granularity**: mostly event-based + rollups; avoid full per-tick per-entity logs by default.
- **Selective high-detail capture**: user wants opt-in targeting by **species**, **entity IDs**, and **genome lineage**.
- **Tests**: add automated tests (GTest + optional Python checks).

### Current Implementation Snapshot (Evidence)
- Telemetry outputs currently exist under `telemetry/`:
  - `telemetry/metrics.csv`
  - `telemetry/events.jsonl`
  - `telemetry/brain_dump.jsonl`
  - `telemetry/brain_activations.csv`
  - `telemetry/traits.csv`
  - `telemetry/speciation.csv`
  References:
  - `sim/include/evolution/sim/telemetry_system.h`
  - `sim/src/telemetry/telemetry_system.cpp`
  - `sim/include/evolution/sim/components.h` (TelemetryComponent)
  - `sim/include/evolution/sim/environment/environment.h` (FeedingStatistics)
  - `sim/src/environment/feeding_system.cpp` (feeding telemetry)
  - `sim/src/systems/metabolism_system.cpp` (death telemetry)
  - `sim/src/main.cpp` (wiring)

### Documentation Divergence (Must Fix During Redesign)
`documentation/modules/telemetry.md` describes `TelemetryConfig`, log rotation, `SystemPerf`, and a different JSON schema; these do not match current code.
Reference: `documentation/modules/telemetry.md`, `sim/src/telemetry/telemetry_system.cpp`.

### Metis Review (Guardrails + Missing Pieces)
- Species system uses NEAT compatibility clustering; **speciation is implicit and not logged**.
  - `sim/src/systems/species_index_system.cpp`
  - `sim/include/evolution/sim/species_index_system.h`
- Genomes already have **two-parent lineage** (`parents[2]`), but there is **no lineage export**.
  - `sim/src/systems/evolution_system.cpp`
  - `sim/src/genetics/genome_ops.cpp`

Defaults applied (override later if needed):
- Treat current `SpeciesId` assignments (compatibility-based) as the primary “species” definition for analytics v1.
- Add explicit telemetry events for **species create/extinct** based on assignments over time.
- Export lineage as a parent-edge list (child GenomeId → parent GenomeIds) and reconstruct DAG in Python.

---

## Work Objectives

### Core Objective
Make telemetry outputs analysis-grade: stable schemas, clear semantics, low overhead, and a Python+DuckDB pipeline that produces Parquet datasets and notebook insights focused on species dynamics and behavior patterns.

### Concrete Deliverables
- Telemetry schema v2 (events/metrics/species rollups) with explicit versioning.
- Selective high-detail capture controls (species/entity/lineage targeting).
- Species-dynamics eventing (species created/extinct; per-species time-series rollups).
- Python analytics pipeline: ingest current outputs → produce Parquet → query with DuckDB.
- Notebook templates for species dynamics + behavior patterns.
- Automated tests validating telemetry output correctness and schema stability.
- Updated documentation aligned with implementation.

### Definition of Done
- Build + tests pass:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build -j`
  - `ctest --test-dir build`
- A sample run produces telemetry outputs; Python pipeline converts to Parquet; notebooks load Parquet and generate at least:
  - per-species population/time series
  - energy flow breakdown (plants vs hunting vs scavenging)
  - behavior proxy metrics (movement/feeding/attack frequencies) at rollup granularity

### Must Have
- Event schemas are versioned and backward-compatible.
- Default logging remains lightweight (no full per-tick per-entity dumps unless opted-in).
- Species dynamics slice is supported end-to-end (sim emits signals; pipeline ingests; notebooks visualize).

### Must NOT Have (Guardrails)
- No always-on full trajectories for all entities.
- No silent schema drift (every output file format change requires a version bump and an ingest update).
- No big new C++ dependencies for Parquet initially (Parquet generation in Python).
- Avoid per-tick allocations and heavy logging in hot loops (respect existing perf guidance).

---

## Verification Strategy

### Test Decision
- **Infrastructure exists**: YES (GoogleTest via CMake)
  - Evidence: `CMakeLists.txt` includes `gtest_discover_tests(sim_tests)`
- **User wants tests**: YES
- **Python checks**: optional but recommended for Parquet/ingest validity

### Automated (GTest)
- Add unit/integration tests that:
  - validate telemetry schema headers (CSV columns) and JSONL required fields
  - validate event emission on known scenarios (e.g., feeding → FEEDING event; death → DEATH event)
  - validate species assignment tracking produces species rollups/events deterministically

### Automated (Python)
- Add a minimal “ingest smoke test” that:
  - reads small sample telemetry outputs
  - writes Parquet
  - runs a few DuckDB queries and asserts row counts / required columns

Recommended deps (pin versions once chosen):
- `duckdb`, `pandas`, `pyarrow`, `numpy`, `matplotlib` (optional: `seaborn`)

### Manual QA (always)
- Run the sim for a short scenario; verify:
  - telemetry directory contains expected outputs
  - notebooks render charts without manual data massaging

---

## Task Flow

```
Schema v2 spec
  → Sim-side emit/rollups + targeting
  → Python ingest (CSV/JSONL → Parquet)
  → Notebooks (species dynamics + behavior patterns)
  → Tests + docs
```

## Parallelization

| Group | Tasks | Reason |
|------:|-------|--------|
| A | 1–2 | Spec writing + file/schema inventory can proceed quickly |
| B | 3–6 | Sim-side telemetry changes (depends on schema) |
| C | 7–9 | Python ingest + notebooks (depends on schema, not on sim performance tuning) |
| D | 10–12 | Tests + docs (once formats settle) |

---

## TODOs

### 0. Baseline & Inventory (Lock current behavior)

**What to do**:
- Enumerate current outputs, columns, and JSONL fields produced by:
  - `telemetry/metrics.csv`
  - `telemetry/events.jsonl`
  - `telemetry/brain_dump.jsonl`
  - `telemetry/brain_activations.csv`
  - `telemetry/traits.csv`, `telemetry/speciation.csv`
- Capture as a markdown “telemetry format snapshot” in-repo for reference.

**Parallelizable**: YES (with 1)

**References**:
- `sim/src/telemetry/telemetry_system.cpp` (file open + write paths)
- `sim/include/evolution/sim/telemetry_system.h` (event types)

**Acceptance Criteria**:
- A documented inventory exists listing each output file, its schema, and how/when it is written.

---

### 1. Telemetry Schema v2 Spec (Events + Rollups)

**What to do**:
- Define stable, versioned schemas:
  - `events_v2.jsonl` (or keep `events.jsonl` but include `schema_version` field)
  - `metrics_v2.csv` (or versioned header)
  - `species_rollups_v2.parquet` target schema (for Python ingest)
- Define required fields for analytics slices:
  - species dynamics: `time/tick`, `species_id`, `population`, `mean_energy`, `deaths_by_cause`, `feeding_energy_by_source`
  - behavior proxies: `distance_traveled`, `feed_count`, `attack_count`, `success_rates` (based on available signals)
- Define event taxonomy mapping from current code and new events:
  - existing: DEATH, FEEDING, SPAWN
  - add: SPECIES_CREATED, SPECIES_EXTINCT (v1), optionally ATTACK (if available)

**Must NOT do**:
- Do not introduce per-tick per-entity dumps as required fields.

**Parallelizable**: YES (with 0)

**References**:
- `documentation/modules/telemetry.md` (doc mismatch list; rewrite during later tasks)
- `sim/src/telemetry/telemetry_system.cpp` (current fields)

**Acceptance Criteria**:
- A written schema spec exists (with field lists, types, and versioning rules).

---

### 2. Add Targeting + Sampling Controls (Species/Entity/Lineage)

**What to do**:
- Add config plumbing to enable:
  - high-detail capture for selected `SpeciesId` values
  - capture for selected entity IDs
  - capture for selected GenomeId lineage (based on parent edges)
- Add sampling strategy:
  - default: rollups only
  - optional: sampled per-entity snapshots for chosen targets at a configurable interval

**References**:
- `sim/include/evolution/sim/telemetry_system.h` (TelemetrySystem ctor/config pattern)
- `sim/src/main.cpp` (how systems are instantiated and wired)
- `sim/include/evolution/sim/species_index_system.h` (`SpeciesId` type)

**Acceptance Criteria**:
- There is a documented way to enable high-detail capture for a chosen species/entity/lineage.

---

### 3. Species Dynamics: Explicit Events + Per-Species Time Series

**What to do**:
- Emit explicit telemetry signals for:
  - species created
  - species extinct
  - (optional) species stagnation milestones
- Produce per-species rollups over time (at a configurable cadence) that can be ingested into Parquet.

**Defaults Applied**:
- Use compatibility-clustering `SpeciesId` as primary definition.

**References**:
- `sim/src/systems/species_index_system.cpp` (assignment logic; implicit new species)
- `sim/include/evolution/sim/species_index_system.h` (Species struct)
- `sim/src/telemetry/telemetry_system.cpp` (add new event writing)

**Acceptance Criteria**:
- Running a short sim produces at least one of:
  - SPECIES_CREATED events when new species appear
  - SPECIES_EXTINCT events when species disappear
- Per-species rollup data exists and is queryable in Python.

---

### 4. Behavior Patterns: Add Proxy Metrics to Rollups

**What to do**:
- Decide which existing signals can serve as behavior proxies (no new brain changes required):
  - movement: distance traveled (TelemetryComponent)
  - feeding: successful_feeds, FEEDING events
  - predation: kill_count, deaths by predation
  - optional: actuation magnitude (if available) or “on_ground/jump” counters
- Add rollups:
  - per-species distributions (mean/p50/p95 where feasible)
  - time windows (1s/10s/60s)

**References**:
- `sim/include/evolution/sim/components.h` (TelemetryComponent fields)
- `sim/src/systems/motor_system.cpp` (distance tracking hook appears here)
- `sim/src/environment/feeding_system.cpp` (feeding events)

**Acceptance Criteria**:
- Notebooks can plot behavior proxies by species over time.

---

### 5. Lineage Export: Parent Edge List (for Python DAG)

**What to do**:
- Export parent edges (child GenomeId → parent GenomeId(s)) as events or a dedicated file.
- Ensure linkage between entity events and genome IDs where possible.

**References**:
- `sim/src/systems/evolution_system.cpp` (offspring.parents = {parent_a, parent_b})
- `sim/src/genetics/genome_ops.cpp` (mutation/crossover context)

**Acceptance Criteria**:
- Python pipeline can reconstruct lineage relationships from exported data.

---

### 6. Schema Alignment Cleanup (Docs + Naming)

**What to do**:
- Decide and enforce naming consistency:
  - prefer `total_energy_*` (current code) or rename to doc naming, but keep stable.
- Update `documentation/modules/telemetry.md` to match the new/actual behavior:
  - remove TelemetryConfig claims unless implemented
  - document actual output paths and formats
  - document sampling/targeting controls

**References**:
- `documentation/modules/telemetry.md`
- `sim/include/evolution/sim/components.h`

**Acceptance Criteria**:
- Docs match reality; no “TelemetryConfig/rotation/SystemPerf” claims unless actually delivered.

---

### 7. Python Ingest: CSV/JSONL → Parquet (DuckDB-friendly)

**What to do**:
- Create Python scripts/notebooks that:
  - read `telemetry/metrics.csv` and `telemetry/events.jsonl`
  - normalize schemas
  - write Parquet datasets (partitioned by run id / scenario / date)
  - include a small “data dictionary” describing columns

Suggested layout (create if missing):
- `analysis/telemetry_ingest/` (scripts + helpers)
- `analysis/notebooks/` (notebooks)
- `analysis/data_dictionary/` (schema docs)

**References**:
- `telemetry/metrics.csv`
- `telemetry/events.jsonl`

**Acceptance Criteria**:
- Running the ingest on a sample dataset produces Parquet files and DuckDB can query them.

---

### 8. Notebook: Species Dynamics Dashboard

**What to do**:
- Provide a notebook that produces:
  - population by species over time
  - extinction/creation markers
  - energy transfer by source (plants/hunting/scavenging)
  - deaths by cause over time

**Acceptance Criteria**:
- Notebook runs end-to-end using Parquet produced by task 7.

---

### 9. Notebook: Behavior Patterns Dashboard

**What to do**:
- Provide a notebook that explores behavior proxies:
  - distance traveled distributions by species
  - feeding rates and predation pressure
  - (optional) clustering of “strategy” using rollup features

**Acceptance Criteria**:
- Notebook runs end-to-end using Parquet produced by task 7.

---

### 10. Tests (GTest): Telemetry Output Contract

**What to do**:
- Add tests to validate:
  - telemetry outputs exist after a short sim run
  - CSV headers contain required columns
  - JSONL events contain required keys and valid enum values
  - deterministic counts for a fixed seed in a small controlled scenario

**References**:
- `tests/sim/` (existing testing patterns, e.g. `tests/sim/test_xor_evolution.cpp`)
- `CMakeLists.txt` (gtest setup)

**Acceptance Criteria**:
- `ctest` runs and telemetry tests pass.

---

### 11. Tests (Python): Parquet Ingest Smoke

**What to do**:
- Minimal script/test that:
  - loads telemetry outputs
  - writes Parquet
  - runs 2–3 DuckDB queries and asserts expected row/column presence

**Acceptance Criteria**:
- A single command (documented) validates ingest correctness on sample data, e.g.:
  - `python -m pytest -q` (if using pytest)
  - or `python analysis/telemetry_ingest/smoke_test.py`

---

### 12. Performance Guardrails

**What to do**:
- Review telemetry emission sites to ensure:
  - no per-tick heavy allocations
  - buffered writes (batch flush)
  - high-detail capture is opt-in and bounded

**References**:
- `AGENTS.md` (repo performance rules)
- `sim/src/telemetry/telemetry_system.cpp`

**Acceptance Criteria**:
- Profiling guidance documented; telemetry overhead stays bounded in default mode.

---

## Commit Strategy
- Prefer small commits per subsystem:
  - telemetry schema + emit changes
  - species dynamics events
  - python ingest
  - notebooks
  - tests
  - docs

---

## Success Criteria
- Deep offline analytics (species dynamics + behavior patterns) is usable via notebooks without manual cleanup.
- Selective high-detail capture exists for species/entity/lineage.
- Telemetry schemas are explicit, versioned, and tested.
