# Next Big Features — Evolution Simulation Roadmap

> **Baseline**: 106/106 tests passing, deterministic fixed-timestep simulation, stable ECS pipeline with staged scheduler.
>
> **Date**: 2026-02-16
>
> **Philosophy**: Each feature below is a **large, self-contained epic**. Every epic ships with comprehensive unit tests *and* end-to-end integration tests. The rule is: **if it isn't tested, it doesn't exist.** We target at minimum a 2:1 test-to-implementation-file ratio per epic.

---

## Priority Matrix

| # | Epic | Impact on Ecosystem Realism | Complexity | Dependencies |
|---|------|-----------------------------|------------|--------------|
| 1 | **Sensory Perception System** | ★★★★★ | High | Brain inference, Physics |
| 2 | **Creature Spatial Index & Advanced Predation** | ★★★★★ | Medium-High | Feeding system, Components |
| 3 | **Population Dynamics & Carrying Capacity** | ★★★★ | Medium | Metabolism, Reproduction, Stats |
| 4 | **Day/Night Climate Cycle** | ★★★★ | Medium | Soil, Plants, Metabolism |
| 5 | **Ecosystem Analytics Dashboard** | ★★★ | Medium | Stats, Telemetry |

---

## Epic 1: Sensory Perception System

### Problem Statement

Creatures currently have no awareness of their environment. The brain inference system receives some contextual inputs (slope, soil nutrient, on-ground flag), but creatures cannot *see* other creatures, *see* nearby plants, or *feel* contact forces. Without perception, brain outputs are essentially random walks rather than informed decisions. This is the single biggest bottleneck preventing emergent intelligent behavior.

### Feature Specification

#### 1.1 Vision System — Raycast Perception

Implement a radial raycast-based vision system that gives each creature a fixed-resolution sensory field.

**Components:**
- `VisionComponent` — configurable per-creature (from genome traits):
  - `fov_degrees` (field of view, default 120°)
  - `num_rays` (ray count, default 8–16)
  - `max_range` (maximum sight distance in meters, default 15.0)
  - `eye_height_offset` (vertical offset from transform, default 0.3)
- `VisionResult` — per-tick output buffer:
  - For each ray: `distance_normalized` [0,1], `hit_type` enum {Nothing, Plant, Herbivore, Carnivore, Terrain}, `hit_energy_fraction` [0,1]
  - Total buffer size: `num_rays * 3` doubles fed to brain input

**System: `VisionSystem`**
- Stage: **PrePhysics** (after spatial indexes are rebuilt, before brain inference)
- Casts rays from creature position + eye offset, fanning out across the FOV in the XZ plane
- Uses the existing `PlantSpatialIndex` for plant hits
- Uses the new `CreatureSpatialIndex` (see Epic 2) for creature hits
- Uses `Terrain::height()` for terrain/heightfield intersections
- Deterministic: ray directions are computed from entity heading (derived from velocity or a new `HeadingComponent`)

**Brain Integration:**
- `BrainInferenceSystem` input vector is extended with vision buffer
- Vision features are appended after existing context features
- Input size becomes: `kContextFeatureCount + num_rays * 3`

#### 1.2 Contact/Touch Sense

**Components:**
- `ContactSenseComponent` — stores last-tick contact events:
  - `contact_count` (how many collision contacts this tick)
  - `contact_normal_sum` (average contact normal direction)
  - `contact_force_magnitude` (total impulse magnitude from solver)

**System integration:**
- The physics solver already computes contact manifolds. After solving, write contact summary data into `ContactSenseComponent` for entities that have one.
- This feeds 3–4 additional brain inputs: contact_count_normalized, contact_normal.x, contact_normal.z, force_magnitude_normalized.

#### 1.3 Internal Sense (Proprioception)

Feed the creature's own metabolic state to the brain:
- `energy_fraction` = energy / max_energy
- `hunger_signal` = 1.0 - energy_fraction (so brain "feels" hunger)
- `age_fraction` = age / max_lifespan (if lifecycle stages define one)
- `speed_magnitude` = |velocity| / max_speed

These 4 inputs are already partially available. Formalize them as a `ProprioceptionComponent` or simply compute inline in `BrainInferenceSystem`.

### Deliverables

| Deliverable | File(s) |
|-------------|---------|
| `VisionComponent`, `VisionResult` added to `components.h` | `include/evolution/sim/components.h` |
| `HeadingComponent` (derived from velocity) | `include/evolution/sim/components.h` |
| `ContactSenseComponent` | `include/evolution/sim/components.h` |
| `VisionSystem` implementation | `src/perception/vision_system.cpp`, `include/.../vision_system.h` |
| `ContactSenseSystem` or solver integration | `src/physics/solver.cpp` modification |
| Brain input vector extension | `src/brain_inference_system.cpp` modification |
| Genome trait mapping for vision params | `src/genetics/derived_traits.cpp` modification |
| Phenotype builder wiring | `src/genetics/phenotype_builder.cpp` modification |

### Testing Requirements

**Unit tests** (`tests/sim/test_vision_system.cpp`):
- Ray casting returns correct distance for a single plant at known position
- Ray casting returns Nothing for empty directions
- FOV clipping: targets outside FOV are not detected
- Range clipping: targets beyond max_range return Nothing
- Hit type classification is correct (Plant vs Herbivore vs Carnivore)
- Vision buffer size matches `num_rays * 3`
- Determinism: same configuration produces identical vision results across runs

**Unit tests** (`tests/sim/test_contact_sense.cpp`):
- Contact data is correctly summarized from solver manifolds
- Zero contacts when no collision occurs
- Multiple contacts from different entities aggregate correctly

**Integration / E2E tests** (`tests/sim/test_perception_integration.cpp`):
- Full scenario tick with vision-equipped creatures produces non-zero vision inputs
- Brain inference system correctly reads extended input vector
- Creature placed near plant has at least one ray returning hit_type=Plant
- Two creatures in proximity detect each other via vision
- Determinism: 100-tick run with perception produces identical telemetry on re-run

### Acceptance Criteria

- [ ] Vision system is registered in `setup_scenario` at `PrePhysics` stage
- [ ] Brain input vector is dynamically sized based on creature's `num_rays`
- [ ] All new code has >90% branch coverage in unit tests
- [ ] E2E test confirms creatures with vision can find and approach plants faster than without (statistical validation over 200 ticks with fixed seed)
- [ ] No regression: all 106 existing tests still pass

---

## Epic 2: Creature Spatial Index & Advanced Predation

### Problem Statement

The current carnivore feeding path does an O(N) scan of all entities every tick for every carnivore. With 200+ creatures this becomes O(N²) per tick, which is unsustainable. Beyond performance, the predation model is simplistic: energy is silently transferred without any concept of pursuit, escape, health, or death-by-predation.

### Feature Specification

#### 2.1 Creature Spatial Index

Mirror the existing `PlantSpatialIndex` design for creatures.

**Class: `CreatureSpatialIndex`**
- Grid-based spatial hash (reuse the same cell_size approach from `PlantSpatialIndex`)
- Rebuild once per tick from all entities with `TransformComponent` + `MetabolismComponent`
- `for_each_in_radius(registry, position, radius, callback)` API
- Stored as `registry.ctx()` singleton, just like `PlantSpatialIndex`

**System: `CreatureSpatialSystem`**
- Stage: **PrePhysics** (before feeding, before vision)
- Clears and rebuilds the index each tick

#### 2.2 Advanced Predation Model

Replace the naive carnivore feeding with a multi-phase predation model:

**Phase 1 — Detection** (uses Vision from Epic 1, or fallback to spatial query):
- Carnivore identifies nearest valid prey within vision range
- Stores target in `CombatComponent::target`

**Phase 2 — Pursuit**:
- `PursuitComponent` tracks chase state:
  - `target_entity`, `pursuit_time`, `max_pursuit_time`
  - If target moves out of range or pursuit times out → disengage
- Brain receives target-relative bearing + distance as additional inputs

**Phase 3 — Attack**:
- Requires `distance < attack_reach` AND `attack_timer <= 0`
- Damage model: `damage = attack_power * dt` (attack_power from genome traits)
- Prey loses `health` (new field on `MetabolismComponent` or separate `HealthComponent`)
- When prey health ≤ 0: prey dies, carnivore gains `prey_energy * conversion_efficiency`

**Phase 4 — Prey Response**:
- Prey receives `ThreatComponent` tag when targeted
- Brain can sense threat via proprioception ("am I being chased?")
- Flee behavior emerges from brain learning, not hardcoded

**Components:**
- `PursuitComponent` { target, pursuit_time, max_pursuit_time, engage_distance }
- `HealthComponent` { health, max_health, regen_rate } (or extend MetabolismComponent)
- `ThreatComponent` { attacker_entity, threat_direction }
- `AttackPowerTrait` derived from genome

### Deliverables

| Deliverable | File(s) |
|-------------|---------|
| `CreatureSpatialIndex` class | `include/.../environment/creature_spatial_index.h`, `src/environment/creature_spatial_index.cpp` |
| `CreatureSpatialSystem` | Same files |
| `PursuitComponent`, `HealthComponent`, `ThreatComponent` | `components.h` |
| Refactored `FeedingSystem` with multi-phase predation | `src/environment/feeding_system.cpp` |
| Genome-derived attack power trait | `src/genetics/derived_traits.cpp` |
| Phenotype builder wiring for predation components | `src/genetics/phenotype_builder.cpp` |

### Testing Requirements

**Unit tests** (`tests/sim/test_creature_spatial_index.cpp`):
- Insert N creatures, query radius returns correct subset
- Empty index returns no results
- Creature at exact boundary of radius is included
- Index handles entity destruction gracefully (stale handles)
- Rebuild produces identical results for identical positions
- Performance: 1000-creature rebuild completes in <1ms (benchmark test)

**Unit tests** (`tests/sim/test_advanced_predation.cpp`):
- Carnivore within attack reach deals damage proportional to dt
- Prey health decrements correctly; death triggers at health ≤ 0
- Attack cooldown prevents rapid-fire attacks
- Pursuit timeout causes disengage
- Energy transfer on kill uses conversion_efficiency correctly
- Carnivore cannot attack other carnivores (diet filter)

**Integration / E2E tests** (`tests/sim/test_predation_e2e.cpp`):
- Spawn 5 herbivores + 1 carnivore in small area: at least one herbivore dies within 500 ticks
- Prey entity is destroyed on death and entity count decreases
- Spatial index survives creature death mid-tick without crash
- Full 1000-tick determinism test with predation active
- Stats system correctly reports predation kill count (new metric)

### Acceptance Criteria

- [ ] Carnivore feeding no longer uses O(N) scan; uses `CreatureSpatialIndex`
- [ ] Prey can die from predation (entity destroyed or marked dead)
- [ ] Predation events are logged to telemetry JSONL
- [ ] All pursuit/attack parameters are genome-derived (evolvable)
- [ ] No regression on existing 106 tests

---

## Epic 3: Day/Night Climate Cycle

### Problem Statement

The simulation runs in a timeless steady state. There is no concept of day or night, no temperature variation, no seasonal or cyclical pressure on organisms. The soil `regenerate_by_biome` accepts a `climate_mult` parameter that is always 1.0. Without temporal variation, there is no selective pressure for creatures to develop time-aware strategies (e.g., resting at night, feeding during peak nutrient hours).

### Feature Specification

#### 3.1 World Clock

**Component: `WorldClock`** (stored as `registry.ctx()` singleton)
- `day_length_seconds` (simulation seconds per full day cycle, default 120.0)
- `time_of_day` [0.0, 1.0) — fraction of day elapsed (0.0 = dawn, 0.25 = noon, 0.5 = dusk, 0.75 = midnight)
- `day_count` — integer day counter
- `phase()` → enum { Dawn, Day, Dusk, Night } computed from time_of_day

**System: `ClockSystem`**
- Stage: **Bootstrap** or beginning of **PrePhysics**
- Advances `time_of_day` by `dt / day_length_seconds` each tick, wraps at 1.0

#### 3.2 Climate Effects

**Soil regeneration modulation:**
- `climate_mult` varies sinusoidally: `0.5 + 0.5 * sin(2π * time_of_day)` → peaks at noon, troughs at midnight
- Wire into `SoilSystem::tick()` which already calls `regenerate_by_biome`

**Plant growth modulation:**
- `PlantGrowthSystem` growth rate scales by `light_level` = `max(0.1, sin(π * time_of_day))` during day, drops to 0.1 at night
- Plants don't die at night, they just grow very slowly

**Metabolism modulation:**
- Basal metabolic rate increases slightly at night (thermoregulation cost): `basal_rate * (1.0 + 0.2 * night_factor)`
- `night_factor` = `max(0.0, -cos(2π * time_of_day))`

#### 3.3 Brain Awareness

- Add `time_of_day` and `light_level` as 2 additional brain inputs
- Creatures can potentially learn circadian strategies

### Deliverables

| Deliverable | File(s) |
|-------------|---------|
| `WorldClock` struct + `ClockSystem` | `include/.../world_clock.h`, `src/world_clock.cpp` |
| Soil climate modulation | `src/environment/soil_system.cpp` modification |
| Plant light-level scaling | `src/environment/plant_systems.cpp` modification |
| Metabolism night cost | `src/metabolism_system.cpp` modification |
| Brain input extension | `src/brain_inference_system.cpp` modification |
| Scenario wiring | `src/scenario.cpp` modification |

### Testing Requirements

**Unit tests** (`tests/sim/test_world_clock.cpp`):
- Clock advances correctly: after `day_length_seconds` of dt accumulation, `time_of_day` wraps to 0.0
- `day_count` increments on wrap
- `phase()` returns correct enum for each quarter
- Deterministic: identical dt sequences produce identical clock state
- Edge case: very large dt (larger than day_length) handles correctly

**Unit tests** (`tests/sim/test_climate_effects.cpp`):
- Soil regeneration at noon produces higher nutrient than at midnight (same biome, same dt)
- Plant growth rate at noon is higher than at midnight
- Metabolism cost at midnight is higher than at noon
- Light level is always ≥ 0.1 (never fully dark)
- Climate mult matches expected sinusoidal formula ±epsilon

**Integration / E2E tests** (`tests/sim/test_day_night_cycle_e2e.cpp`):
- 240-second simulation (2 full day cycles): soil nutrient mean oscillates with expected period
- Plant biomass (total energy) shows diurnal oscillation pattern
- Creature energy shows nighttime drain relative to daytime gain
- Full determinism check over 2 day cycles
- Stats system reports climate metrics (light_level, climate_mult) at each report interval

### Acceptance Criteria

- [ ] `WorldClock` is accessible via `registry.ctx().get<WorldClock>()`
- [ ] Day/night cycle is configurable via `SimulationScenario`
- [ ] All existing systems that use climate_mult read from WorldClock
- [ ] Brain receives time-of-day input
- [ ] Telemetry outputs include day_count and time_of_day
- [ ] No regression on existing 106 tests

---

## Epic 4: Population Dynamics & Carrying Capacity

### Problem Statement

The current simulation has no population regulation mechanisms beyond energy starvation. Without carrying capacity enforcement, populations can either explode (crashing performance) or collapse to zero (ending the simulation). A healthy ecosystem simulation needs density-dependent feedback loops that emerge from resource competition but are also backstopped by explicit safety mechanisms.

### Feature Specification

#### 4.1 Population Monitor

**Class: `PopulationMonitor`** (stored as `registry.ctx()` singleton)
- Tracks per-tick counts: `creature_count`, `herbivore_count`, `carnivore_count`, `plant_count`
- Computes rolling averages (exponential moving average, window = 30 ticks)
- Detects population crash: `creature_count < min_viable_population` (configurable, default 10)
- Detects population explosion: `creature_count > max_carry_capacity` (configurable, default 500)

#### 4.2 Resource Competition Pressure

Instead of hardcoded population caps, use resource scarcity as the primary regulator:

**Soil depletion feedback:**
- When `mean_nutrient < depletion_threshold` (e.g., 0.2), reduce plant seeding `establish_probability` proportionally
- This naturally limits plant biomass → limits herbivore food → limits herbivore population → limits carnivore food

**Density-dependent reproduction:**
- `ReproductionSystem` applies a density penalty: `effective_energy_threshold = base_threshold * (1.0 + density_pressure)`
- `density_pressure = max(0.0, (local_creature_count / ideal_density) - 1.0)`
- `local_creature_count` is queried from `CreatureSpatialIndex` (Epic 2) within a radius
- This makes reproduction harder in crowded areas

#### 4.3 Safety Nets

**Extinction prevention:**
- If `creature_count < min_viable_population` for `extinction_grace_ticks` consecutive ticks:
  - Spawn `rescue_batch_size` new random creatures (same as initial seeding)
  - Log telemetry event: `"population_rescue"`
  - Increment rescue counter for analysis

**Overpopulation cull:**
- If `creature_count > hard_cap` (e.g., 1000):
  - Remove the lowest-fitness entities until count drops to `target_cap`
  - Log telemetry event: `"overpopulation_cull"`
  - This is a last resort; density-dependent reproduction should prevent this

#### 4.4 Equilibrium Metrics

New telemetry fields emitted per stats interval:
- `creature_density` (creatures per unit area)
- `herbivore_to_plant_ratio`
- `carnivore_to_herbivore_ratio`
- `reproduction_rate` (births per stats interval)
- `death_rate` (deaths per stats interval)
- `population_stability_index` (rolling variance of population count)

### Deliverables

| Deliverable | File(s) |
|-------------|---------|
| `PopulationMonitor` class | `include/.../population_monitor.h`, `src/population_monitor.cpp` |
| `PopulationSystem` (per-tick updates) | Same files |
| Density-dependent reproduction | `src/reproduction_system.cpp` modification |
| Soil depletion → plant seeding feedback | `src/environment/plant_systems.cpp` modification |
| Extinction rescue logic | `src/population_monitor.cpp` |
| Overpopulation cull logic | `src/population_monitor.cpp` |
| Extended stats/telemetry | `src/stats_system.cpp`, `src/telemetry/` modification |
| Scenario configuration fields | `include/.../scenario.h` modification |

### Testing Requirements

**Unit tests** (`tests/sim/test_population_monitor.cpp`):
- Monitor correctly counts creatures, herbivores, carnivores, plants
- EMA rolling average converges to steady value over 60 ticks
- Crash detection triggers at exactly `min_viable_population - 1`
- Explosion detection triggers at exactly `max_carry_capacity + 1`
- Density pressure formula returns 0.0 when below ideal density
- Density pressure formula returns positive value when above ideal density

**Unit tests** (`tests/sim/test_density_reproduction.cpp`):
- High local density increases effective reproduction threshold
- Low local density does not penalize reproduction
- Reproduction is fully blocked when density_pressure exceeds a critical value
- Density query uses spatial index correctly (radius-based count matches expected)

**Unit tests** (`tests/sim/test_population_safety.cpp`):
- Extinction rescue spawns exactly `rescue_batch_size` creatures
- Rescue creatures have valid genomes and full components
- Overpopulation cull removes lowest-fitness entities first
- Cull never removes below `target_cap`
- Rescue event and cull event are logged to telemetry

**Integration / E2E tests** (`tests/sim/test_population_dynamics_e2e.cpp`):
- Start with 50 creatures, run 2000 ticks: population stays within [min_viable, max_carry] bounds
- Start with 5 creatures (below min_viable): rescue triggers within grace period
- Start with 200 creatures in tiny area: density pressure reduces reproduction rate
- Determinism: identical scenario produces identical population trajectory
- Equilibrium metrics are present in stats output

### Acceptance Criteria

- [ ] Population never drops below `min_viable_population` for more than `extinction_grace_ticks`
- [ ] Population never exceeds `hard_cap`
- [ ] Density-dependent reproduction is genome-configurable (the density sensitivity can evolve)
- [ ] All safety events are telemetry-logged
- [ ] Equilibrium metrics available in stats reports
- [ ] No regression on existing tests

---

## Epic 5: Ecosystem Analytics Dashboard

### Problem Statement

Currently the simulation outputs raw JSONL events, CSV metrics, and spdlog console lines. There is no way to visualize population dynamics, species divergence, genome trait distributions, or energy flow in real time. For a simulation that often runs headless for thousands of ticks, post-hoc analytics are essential for tuning and understanding emergent behavior.

### Feature Specification

#### 5.1 Structured Snapshot System

**Class: `SnapshotWriter`**
- At configurable intervals (e.g., every 100 ticks), serialize a full world snapshot:
  - All creature positions, energies, fitness, species_id, genome_id, age
  - All plant positions, energies, alive status
  - Soil grid mean per biome
  - WorldClock state
  - Population counts and ratios
- Output format: Protocol Buffers or flat JSON (one file per snapshot, named by tick number)
- Directory: `{telemetry_output_dir}/snapshots/`

#### 5.2 Species Lineage Tracker

**Class: `LineageTracker`**
- Tracks parent→child relationships for every reproduction event
- Records: `{ child_genome_id, parent_a_genome_id, parent_b_genome_id, tick, species_id }`
- At each snapshot interval, emit a lineage summary:
  - Species count, sizes, mean fitness per species
  - Species birth/death events (new species formed, old species extinct)
- Output: `{telemetry_output_dir}/lineage/` directory with per-interval JSONs

#### 5.3 Post-Hoc Analysis Scripts

Provide Python scripts that read the snapshot + lineage data and produce:

**Script 1: `analyze_population.py`**
- Population over time (line chart: total, herbivore, carnivore, plants)
- Birth/death rates over time
- Population stability index over time

**Script 2: `analyze_species.py`**
- Species count over time
- Phylogenetic tree visualization (from lineage data)
- Trait distribution histograms per species at key time points

**Script 3: `analyze_energy_flow.py`**
- Sankey diagram: soil → plants → herbivores → carnivores → death
- Energy budget: total ecosystem energy over time
- Feeding efficiency metrics

### Deliverables

| Deliverable | File(s) |
|-------------|---------|
| `SnapshotWriter` class | `include/.../snapshot_writer.h`, `src/telemetry/snapshot_writer.cpp` |
| `LineageTracker` class | `include/.../lineage_tracker.h`, `src/telemetry/lineage_tracker.cpp` |
| Reproduction system → lineage event hook | `src/reproduction_system.cpp` modification |
| Scenario wiring for snapshots + lineage | `src/scenario.cpp` modification |
| `analyze_population.py` | `scripts/analyze_population.py` |
| `analyze_species.py` | `scripts/analyze_species.py` |
| `analyze_energy_flow.py` | `scripts/analyze_energy_flow.py` |

### Testing Requirements

**Unit tests** (`tests/sim/test_snapshot_writer.cpp`):
- Snapshot captures correct creature count matching registry
- Snapshot captures correct plant count
- Snapshot includes WorldClock state if present
- Snapshot file naming follows expected pattern
- Empty world produces valid (but empty) snapshot

**Unit tests** (`tests/sim/test_lineage_tracker.cpp`):
- Reproduction event correctly records parent-child relationship
- Species birth event is emitted when a new species_id first appears
- Species extinction event is emitted when last member of species dies
- Lineage data is retrievable by genome_id
- Multiple generations produce correct ancestor chains

**Integration / E2E tests** (`tests/sim/test_analytics_e2e.cpp`):
- 500-tick scenario produces expected number of snapshot files
- Lineage data is consistent: every child's parents exist in the tracker
- Snapshot creature counts match live registry counts at snapshot tick
- Snapshot files are valid JSON and parseable
- Determinism: identical scenario produces byte-identical snapshots

### Acceptance Criteria

- [ ] Snapshots are written at configurable intervals
- [ ] Lineage tracker captures every reproduction event
- [ ] Python scripts produce readable charts from snapshot data
- [ ] Snapshot + lineage data is sufficient to reconstruct population history
- [ ] File I/O does not cause measurable tick slowdown (< 1ms per snapshot)
- [ ] No regression on existing tests

---

## Implementation Order & Dependencies

```
Epic 2 (Creature Spatial Index)  ──┐
                                    ├──→  Epic 1 (Sensory Perception)
Epic 4 (Day/Night Cycle)  ────────┘           │
                                              │
Epic 3 (Population Dynamics)  ←───────────────┘
         │
         └──→  Epic 5 (Ecosystem Analytics)
```

**Recommended execution order:**

1. **Epic 2** first — the `CreatureSpatialIndex` is a prerequisite for both vision raycasting against creatures and density-dependent reproduction. It also immediately fixes the O(N²) carnivore feeding bug.

2. **Epic 1** second — highest behavior leverage once spatial indexing exists: perception unlocks informed movement, feeding, and predation decisions.

3. **Epic 3** third — population controls and carrying-capacity feedback are prioritized over climate visuals because they determine whether long-run evolution remains viable instead of collapsing to extinction.

4. **Epic 4** fourth — day/night pressure is still valuable, but applied after ecosystem stability controls are in place.

5. **Epic 5** last — all the interesting data to visualize comes from Epics 1–4. Build analytics after there is stable long-run simulation behavior worth analyzing.

---

## Testing Strategy Summary

### Test File Naming Convention

All test files follow the pattern `tests/sim/test_<feature_name>.cpp` for unit tests and `tests/sim/test_<feature_name>_e2e.cpp` for end-to-end tests.

### Test Infrastructure Expectations

- Use the existing `test_fixtures.cpp` and `test_fixtures.h` patterns for registry setup
- Every test must be deterministic (fixed seeds, no wall-clock dependencies)
- E2E tests should assert invariants over multi-tick runs (e.g., "after 500 ticks, population is within bounds")
- Performance regression tests should have explicit timing bounds

### Estimated New Test Count

| Epic | Unit Tests | E2E Tests | Total |
|------|-----------|-----------|-------|
| 1. Sensory Perception | ~20 | ~8 | ~28 |
| 2. Creature Spatial + Predation | ~15 | ~8 | ~23 |
| 3. Day/Night Cycle | ~12 | ~6 | ~18 |
| 4. Population Dynamics | ~18 | ~6 | ~24 |
| 5. Ecosystem Analytics | ~10 | ~5 | ~15 |
| **Total** | **~75** | **~33** | **~108** |

This roughly doubles the test suite from 106 to ~214 tests.

---

## Per-Epic Effort Estimates

| Epic | Implementation | Testing | Integration & Polish | Total |
|------|---------------|---------|---------------------|-------|
| 1. Sensory Perception | 3–4 sessions | 2 sessions | 1 session | ~6 sessions |
| 2. Creature Spatial + Predation | 2–3 sessions | 2 sessions | 1 session | ~5 sessions |
| 3. Day/Night Cycle | 1–2 sessions | 1 session | 0.5 session | ~3 sessions |
| 4. Population Dynamics | 2–3 sessions | 2 sessions | 1 session | ~5 sessions |
| 5. Ecosystem Analytics | 2–3 sessions | 1 session | 1 session | ~4 sessions |

> A "session" is roughly one focused coding conversation/period.

---

## Non-Goals (Explicitly Deferred)

These are important but NOT part of this plan:

- **Parallel/multithreaded scheduler** — performance optimization, not feature work
- **Rendering/visualization** — no rendering pipeline exists yet; analytics scripts fill this gap
- **Advanced morphology** — articulated bodies with rotation; the physics solver needs rotation support first
- **Neural architecture search** — NEAT topology evolution; current MLP + NEAT hybrid is sufficient
- **Networked simulation** — client-server or distributed simulation
- **Sound/audio perception** — not enough ecosystem complexity yet to warrant it
