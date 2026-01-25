# Nutrient Return Loop + Social Behaviors (Brain-Driven)

## Context

### Original Request
Build two features:
1) Nutrient return loop: corpse decomposition feeds soil and plant growth over time.
2) Social behaviors: flocking/herding, pack hunting, territorial zones.

### Interview Summary
- Social behaviors: brain-driven signals (add sensors; do not hard-code steering).
- Nutrient return loop: use existing `DecompositionSystem` point injection; focus on wiring (where missing), tuning, tests, telemetry.
- Social behaviors in scope now: herding/flocking + territoriality + pack hunting.
- Verification: TDD (tests are part of each task).

### Codebase Findings (verified)
- Corpse decay + nutrient injection already exists:
  - `sim/src/systems/decomposition_system.cpp` injects nitrogen into `SoilVolume` or `SoilGrid`.
  - `sim/include/evolution/sim/decomposition_system.h` documents the intended behavior.
  - `sim/include/evolution/sim/components.h` defines `CorpseComponent`.
- System scheduling differs across entrypoints:
  - `sim/src/core/scenario.cpp` schedules `DecompositionSystem`.
  - `sim/src/main.cpp` does NOT schedule `DecompositionSystem`.
- Build integration gap (verified):
  - `sim/src/systems/decomposition_system.cpp` is not currently listed in `CMakeLists.txt` sources (so scheduling it may not actually link in the implementation until fixed).
- Soil + plants already form the “forward” half of the loop:
  - `sim/src/environment/soil_system.cpp` updates `SoilGrid`/`SoilVolume`.
  - `sim/src/environment/plant_systems.cpp` consumes soil nutrients to grow plant energy.
- Brain/sensing integration pattern:
  - `sim/src/systems/brain_inference_system.cpp` builds a fixed sensor vector, then appends vision rays.
  - `sim/src/systems/vision_system.cpp` populates `VisionComponent`.
- Brain input sizing is currently hard-coded during genome creation:
  - `sim/src/genetics/genome_storage.cpp` sets MLP/NEAT `input_count = 18` (8 context + 5 ray dist + 5 ray types).
- Vision ray count can vary by genome:
  - `sim/src/genetics/phenotype_builder.cpp` sets `VisionComponent.ray_count` from `genome->sensory()->vision_rays()`.
  - `sim/src/systems/brain_inference_system.cpp` uses `vision->ray_distances.size()` to fill vision slots.
- Efficient neighbor queries foundation exists:
  - `sim/include/evolution/sim/creature_spatial_index.h` implements a grid-hash index with `for_each_in_radius()`.

### Metis Review
- Attempted but tooling unavailable in this environment; plan includes explicit self-guardrails and gap checks instead.

---

## Work Objectives

### Core Objective
Close the nutrient loop consistently across all simulation entrypoints and add a social-signal layer so brains can learn flocking, territoriality, and pack hunting.

### Concrete Deliverables
- Ensure decomposition is active anywhere creatures can die (at least `sim/src/main.cpp` and scenario runner).
- Add tests that prove decomposition increases local soil nutrients deterministically.
- Add a social signaling system (neighbor aggregation + territory + pack cues) feeding into brain inputs without breaking existing genomes.
- Add tests that prove social signals are deterministic and sensible on small toy setups.
- Update architecture docs + data contracts for new system ordering + components.

### Must NOT Have (Guardrails)
- Do not replace brain movement with rule-based flocking/pack steering.
- Do not change existing sensor indices (0..vision) semantics; only append new sensors after current mappings.
- Do not introduce per-entity heap allocations in hot tick paths (no neighbor lists stored per entity).
- Do not add a “microbe pool” model or radius nutrient plume (explicitly out for this iteration).

---

## Verification Strategy (TDD)

### Test Decision
- Infrastructure exists: YES (C++ unit/integration tests under `tests/`).
- User wants tests: YES (TDD).

### Standard Commands (adjust to repo conventions)
- Configure/build: `cmake -S . -B build && cmake --build build`
- Run tests: `ctest --test-dir build`

Important: tests are not auto-discovered. New `tests/*.cpp` files must be added to the explicit `sim_tests` source list in `CMakeLists.txt`.

> Each TODO includes: (1) a failing test first, (2) implementation to green, (3) refactor.

### Sensor Layout Contract (Must be explicit for deterministic tests)

**Single source of truth requirement**:
- Put the sensor-layout constants in one shared header used by BOTH genome generation and brain inference.

Proposed location:
- `sim/include/evolution/sim/brain_io_layout.h`
  - Included by:
    - `sim/src/genetics/genome_storage.cpp`
    - `sim/src/systems/brain_inference_system.cpp`
    - `sim/src/genetics/phenotype_builder.cpp` (for vision-ray clamping)

Define constants used by both genome generation and brain inference:
- `kBaseSensorCount = 8`
- `kVisionRayCapacity` (fixed maximum slots reserved for vision)
- `kSocialSensorCount` (fixed social tail size)

Implied total input size for newly-generated brains:
- `kTotalInputCount = kBaseSensorCount + 2*kVisionRayCapacity + kSocialSensorCount`

Index layout in the brain input vector:
- Base/context sensors: indices `0..7` (unchanged)
- Vision distance block (fixed capacity): indices `8..(8 + kVisionRayCapacity - 1)`
- Vision hit-type block (fixed capacity): indices `(8 + kVisionRayCapacity)..(8 + 2*kVisionRayCapacity - 1)`
- Social tail (fixed count): indices `(8 + 2*kVisionRayCapacity)..(8 + 2*kVisionRayCapacity + kSocialSensorCount - 1)`

Filling rules:
- For this iteration, clamp `VisionComponent.ray_count` to exactly `kVisionRayCapacity`.
- Social sensors are filled only if the social component exists; otherwise zeros.

Defaults Applied:
- `kVisionRayCapacity = 5` (matches current generator comment; keep stable this iteration)
- `kSocialSensorCount = 12`
- `kTotalInputCount = 30`

Social sensor ordering (12 scalars, append-only tail):
1. cohesion_dir_x [-1, 1]
2. cohesion_dir_z [-1, 1]
3. alignment_dir_x [-1, 1]
4. alignment_dir_z [-1, 1]
5. separation_dir_x [-1, 1]
6. separation_dir_z [-1, 1]
7. neighbor_density [0, 1]
8. territory_dist_norm [0, 1]
9. intruder_density [0, 1]
10. prey_dir_x [-1, 1] (toward chosen prey)
11. prey_dir_z [-1, 1]
12. pack_density_near_prey [0, 1]

---

## Task Flow

1) Decomposition verification + wiring (unblocks “nutrient loop everywhere”)
2) Social signal plumbing (new components + spatial index lifecycle)
3) Brain sensor integration (append-only)
4) Integration + docs

---

## TODOs

- [ ] 0. Build/link sanity: ensure DecompositionSystem is compiled in all targets

  **Why**:
  - `DecompositionSystem` can be scheduled but still be a no-op if its `.cpp` is not linked into the binary.
  - Verified current-state issue: `CMakeLists.txt` does not reference `decomposition_system.cpp` today.

  **What to do**:
  - Verify `sim/src/systems/decomposition_system.cpp` is included in the `sim_core` target sources in `CMakeLists.txt`.
  - Add missing sources to `CMakeLists.txt` (and, later, any new social system sources added by this plan).
  - Expect an immediate compile error once the file is actually built (current `.cpp` uses `SoilGrid` but does not include the header that defines it).
    - Intended fix: include `sim/include/evolution/sim/environment/environment.h` (or another canonical header that defines `SoilGrid`) in `sim/src/systems/decomposition_system.cpp`.

  **References**:
  - `CMakeLists.txt` - authoritative list of compiled sources.
  - `sim/src/systems/decomposition_system.cpp` - must be compiled/linked.

  **Acceptance Criteria**:
  - `cmake -S . -B build && cmake --build build` succeeds.

- [ ] 1. Add shared brain I/O layout constants header (single source of truth)

  **What to do (RED)**:
  - Add a small unit test that asserts:
    - `kTotalInputCount == 30` (or whatever constant you choose),
    - `kVisionRayCapacity == 5`,
    - `kSocialSensorCount == 12`.
  - This test should fail until the constants exist in the shared header.

  **What to do (GREEN)**:
  - Add `sim/include/evolution/sim/brain_io_layout.h` defining the constants used by this plan.
  - Update all call sites to include this header instead of duplicating numbers/comments.

  **Acceptance Criteria**:
  - The constants compile and are referenced by:
    - genome generation,
    - phenotype builder vision config,
    - brain inference.

- [ ] 2. Add Scheduler introspection for ordering tests (minimal, non-invasive)

  **Why**:
  - There is currently no way for tests to verify system registration order (no public access to `Scheduler::systems_`).

  **What to do (RED)**:
  - Add a test that registers a few dummy systems and asserts their name order.
  - This test should fail until an introspection API exists.

  **What to do (GREEN)**:
  - Add a small API to `Scheduler`:
    - e.g., `std::vector<std::string_view> system_names() const;`
  - Keep it read-only and low risk (no mutation access).

  **References**:
  - `sim/include/evolution/sim/scheduler.h` - add the API.
  - `sim/src/core/scheduler.cpp` - implement by iterating `systems_` and returning `system->name()`.

  **Acceptance Criteria**:
  - Ordering test passes deterministically.

- [ ] 3. Prove DecompositionSystem nutrient injection works (unit test)

  **What to do (RED)**:
  - Add a focused test that:
    - Creates a registry with `SoilGrid` (or `SoilVolume`) in `registry.ctx()`.
    - Spawns one entity with `TransformComponent` + `CorpseComponent`.
    - Ticks `DecompositionSystem` with known `dt`.
    - Asserts: corpse biomass decreases and the corresponding soil cell increases by the same amount (no clamping; current behavior).

  **What to do (GREEN)**:
  - If test fails due to missing setup utilities, add minimal test fixture helpers.
  - Fix any determinism/units mismatches uncovered by the test.

  **References**:
  - `sim/src/systems/decomposition_system.cpp` - point injection logic and coordinate-to-cell mapping.
  - `sim/include/evolution/sim/decomposition_system.h` - intended contract.
  - `sim/include/evolution/sim/components.h` - `CorpseComponent` fields and semantics.
  - `sim/include/evolution/sim/environment/environment.h` - `SoilGrid::at()` and nutrient storage.

  **Acceptance Criteria**:
  - `ctest --test-dir build` includes a new failing test first (RED), then passes (GREEN).
  - Test asserts exact or near-exact delta for biomass loss vs soil gain.

- [ ] 4. Wire DecompositionSystem into `sim/src/main.cpp` scheduler (and keep ordering sane)

  **What to do (RED)**:
  - Add (or extend) an integration test that mirrors the `sim/src/main.cpp` system registration order and verifies a corpse decays + soil increases over multiple ticks.
  - Use the existing ordering-test style as a template:
    - `tests/sim/test_integration_system_ordering.cpp`
  - The test should fail before wiring because decomposition is not scheduled.

  **What to do (GREEN)**:
  - Schedule `DecompositionSystem` in `sim/src/main.cpp`.
  - Place it after movement decisions (Brain/Motor) and before metabolism cleanup, matching the scenario runner pattern.

  **References**:
  - `sim/src/core/scenario.cpp` - known-good system ordering includes `DecompositionSystem`.
  - `sim/src/main.cpp` - missing `DecompositionSystem` in current ordering.
  - `sim/src/systems/metabolism_system.cpp` - creates corpses on death (so decomposition can process them).

  **Acceptance Criteria**:
  - New integration test fails before wiring and passes after.
  - `ctest --test-dir build` passes.

- [ ] 5. Add decomposition telemetry counters (optional but recommended)

  **What to do (RED)**:
  - Add a test that after a decomposition tick, telemetry/stats reflects:
    - total corpse biomass (already aggregated), and
    - a new “nutrients returned this tick” counter (if added).

  **What to do (GREEN)**:
  - Add a new `DecompositionStatistics` context resource (stored in `registry.ctx()`) with at least:
    - `double biomass_decayed_last_tick`
    - `double nutrients_returned_last_tick` (should match biomass decayed)
  - Reset it at the start of `DecompositionSystem::tick()` and increment by the decomposition “loss”.

  **References**:
  - `sim/src/telemetry/telemetry_system.cpp` - existing biomass aggregation includes corpses (optional cross-check).
  - `sim/src/systems/decomposition_system.cpp` - has the exact “loss” to record.

  **Acceptance Criteria**:
  - Counter increases deterministically with fixed dt.
  - The test reads the counter directly from `registry.ctx()` (no filesystem/log parsing).
  - `ctest --test-dir build` passes.

- [ ] 6. Define social signal data model (components + contracts)

  **What to do (RED)**:
  - Add compile-time/unit tests asserting:
    - new component types are POD-like (as per repo patterns),
    - default values are deterministic,
    - no dynamic allocations per entity.

  **What to do (GREEN)**:
  - Add these exact components in `sim/include/evolution/sim/components.h`:
    - `struct TerritoryComponent`:
      - `Vec3 center` (initialized once)
      - `double radius` (initialized to `territory_radius` constant)
      - `bool initialized` (default false)
    - `struct SocialSignalsComponent`:
      - `Vec3 cohesion_dir`
      - `Vec3 alignment_dir`
      - `Vec3 separation_dir`
      - `double neighbor_density`
      - `double territory_dist_norm`
      - `double intruder_density`
      - `Vec3 prey_dir`
      - `double pack_density_near_prey`
  - Keep storage as fixed-size scalars and `Vec3` only.

  **Objective “no allocations” verification**:
  - Add `static_assert(std::is_trivially_copyable_v<...>)` / `static_assert(std::is_standard_layout_v<...>)` for the new components.
  - Ensure the new components contain no `std::vector`, `std::string`, `std::unordered_*`, etc.

  **Initialization lifecycle (must be explicit)**:
  - `SocialBehaviorSystem` will `get_or_emplace` missing `TerritoryComponent`/`SocialSignalsComponent` for any creature it processes.
  - Territory center initialization rule:
    - On first tick where `TerritoryComponent.initialized == false`, set `center = TransformComponent.position` and mark `initialized = true`.
  - This ensures existing in-world entities get deterministic defaults without requiring spawn-path changes.

  **References**:
  - `sim/include/evolution/sim/components.h` - patterns for new components and tags.
  - `documentation/architecture/overview.md` - must update component list.
  - `documentation/data-contracts/inter_module_contracts.md` - must document new data flow.

  **Acceptance Criteria**:
  - New components compile and are used in at least one test.

- [ ] 7. Add a CreatureSpatialIndex lifecycle for neighbor queries (system or per-tick resource)

  **What to do (RED)**:
  - Add a unit test that:
    - spawns 3 creatures in known positions,
    - rebuilds the index,
    - queries within a radius,
    - expects deterministic membership results.

  **What to do (GREEN)**:
  - Implement a dedicated `CreatureSpatialIndexSystem` that rebuilds an index stored in `registry.ctx()` once per tick.

  **Defaults Applied**:
  - Rebuild once per tick early in the “creature behavior” portion of the schedule.

  **References**:
  - `sim/include/evolution/sim/creature_spatial_index.h` - rebuild + `for_each_in_radius()`.
  - `sim/src/environment/feeding_system.cpp` - currently rebuilds a local CreatureSpatialIndex each tick (pattern/anti-pattern to consider).

  **Acceptance Criteria**:
  - Unit test passes; query results stable across runs.
  - No per-entity allocations.

- [ ] 8. Implement SocialBehaviorSystem (compute social signals; do not steer)

  **Social Signal Math Contract (no-guesswork for TDD)**:
  - Neighbor eligibility (for cohesion/alignment/separation):
    - include: entities with `TransformComponent`, `KinematicsComponent`, `MetabolismComponent`, `DietComponent` and `energy > 0`.
    - filter: same `DietType` as self.
    - exclude: self; corpses (even though `CreatureSpatialIndex` can index them).
  - Radii/constants (fixed for this iteration):
    - `neighbor_radius = 10.0`
    - `separation_radius = 3.0`
    - `territory_radius = 12.0`
    - `max_prey_radius = 20.0`
    - `near_prey_radius = 6.0`
    - density normalizers: `kMaxNeighborsForDensity = 8`, `kMaxIntrudersForDensity = 4`, `kMaxPackForDensity = 4`
  - Vector math (XZ plane only; Y=0):
    - `cohesion_dir = normalize(centroid(neighbors).xz - self_pos.xz)` else (0,0)
    - `alignment_dir = normalize(mean(neighbor_vel.xz))` else (0,0)
    - `separation_dir = normalize(sum_{n within separation_radius}(normalize(self_pos.xz - n_pos.xz) / max(dist, eps)))` else (0,0)
      - use `eps = 1e-6`
  - Densities:
    - `neighbor_density = clamp(neighbor_count / kMaxNeighborsForDensity, 0, 1)`
    - `territory_dist_norm = clamp(distance(self_pos.xz, territory_center.xz) / territory_radius, 0, 1)`
    - `intruder_density = clamp(intruder_count / kMaxIntrudersForDensity, 0, 1)`
    - intruders: alive entities whose position is within `territory_radius` of `TerritoryComponent.center` and with `DietType != self.DietType`
  - Pack hunting cues (carnivores only):
    - prey selection: nearest alive herbivore within `max_prey_radius` (if none: prey_dir=0, pack_density_near_prey=0)
    - `prey_dir = normalize(prey_pos.xz - self_pos.xz)`
    - allies: other alive carnivores
    - `pack_density_near_prey = clamp(ally_count_within(near_prey_radius of prey) / kMaxPackForDensity, 0, 1)`

  **What to do (RED)**:
  - Write deterministic toy tests that assert exact outcomes implied by the above contract.

  **Alive-only filtering implementation rule (no guesswork)**:
  - Do NOT use `CreatureSpatialIndex::find_nearest()` for prey selection, because it can return corpses.
  - Instead:
    - iterate candidates via `CreatureSpatialIndex::for_each_in_radius()`, and
    - filter by presence of `MetabolismComponent` and `energy > 0`, and
    - filter prey by `HerbivoreTag` (matching existing predation expectations in `sim/src/environment/feeding_system.cpp`).

  **What to do (GREEN)**:
  - Add a new system that:
    - uses CreatureSpatialIndex to iterate neighbors within radius,
    - computes aggregates deterministically (always sort),
    - writes results into the social component.

  **Determinism Guardrail**:
  - Always enforce a stable order before aggregation:
    - collect candidate neighbor entity ids into a scratch buffer owned by the system instance,
    - sort ascending by entity id,
    - then compute aggregates.
  - Do not store neighbor lists on components.

  **Performance guardrail (no per-entity allocations)**:
  - The system owns scratch buffers as members (e.g., `std::vector<entt::entity> scratch_neighbors_`).
  - `scratch_neighbors_.reserve(64)` (or another justified upper bound) during construction.
  - Per entity, use `scratch_neighbors_.clear()` (no reallocation).

  **References**:
  - `sim/include/evolution/sim/creature_spatial_index.h` - neighbor iteration API.
  - `sim/src/systems/brain_inference_system.cpp` - pattern: optional sensors that fill only when inputs exist.
  - `sim/include/evolution/sim/math_types.h` - Vec3 math helpers.

  **Acceptance Criteria**:
  - Tests prove signal directionality + deterministic repeatability.
  - `ctest --test-dir build` passes.

- [ ] 9. Append social sensors into BrainInferenceSystem (append-only mapping)

  **What to do (RED)**:
  - Add a test that creates an entity with sufficiently large `BrainComponent.input_count` and asserts:
    - base sensors remain unchanged,
    - vision sensors remain in the same slots,
    - social sensors appear only after vision slots.

  **What to do (GREEN)**:
  - Extend `BrainInferenceSystem` to:
    - treat the vision block as fixed-capacity `kVisionRayCapacity`, and
    - append the 12 social sensors after the fixed vision blocks.
  - Use simple normalization ([-1,1] for vector components, [0,1] for counts/scalars).

  **References**:
  - `sim/src/systems/brain_inference_system.cpp` - current index layout: [0..7] base, [8..] vision distances + hit types.
  - `sim/include/evolution/sim/components.h` - `ActuationComponent` pattern (existing outputs).

  **Acceptance Criteria**:
  - Test proves no existing indices shift.
  - `ctest --test-dir build` passes.

- [ ] 10. Increase newly-generated brain input_count to include social sensors

  **Why**:
  - Without increasing `input_count`, newly computed social signals will never be visible to brains created by `GenomeStorage::create_random()`.

  **What to do (RED)**:
  - Add a test that creates a new random genome and asserts its `MLP/NEAT.input_count` matches the expected "base + vision + social" total.

  **What to do (GREEN)**:
  - Update genome generation to allocate enough inputs for appended social sensors.
  - Keep the existing base/vision layout; social sensors occupy the new tail slots.

  **Defaults Applied**:
  - Define and use:
    - `kVisionRayCapacity = 5`
    - `kSocialSensorCount = 12`
  - Use them in:
    - genome generation (`PopulateMlp`/`PopulateNeat`), and
    - brain inference (to fill those slots deterministically).

  **References**:
  - `sim/src/genetics/genome_storage.cpp` - `PopulateMlp()` / `PopulateNeat()` set `input_count = 18` today.
  - `sim/src/systems/brain_inference_system.cpp` - must append sensors only if `input_count` allows.

  **Acceptance Criteria**:
  - Test proves new genomes expose the new social sensor tail.
  - `ctest --test-dir build` passes.

- [ ] 11. Lock vision ray count to the capacity for this iteration (avoid index-mapping drift)

  **Why**:
  - The existing generator assumes 5 rays (see comment in `PopulateMlp/PopulateNeat`).
  - Keeping `VisionComponent.ray_count == 5` ensures we do not accidentally change sensor index semantics for any existing genomes/brains.

  **What to do (RED)**:
  - Add a test that builds a phenotype from a genome and asserts `VisionComponent.ray_count == kVisionRayCapacity`.

  **What to do (GREEN)**:
  - Clamp `vision_rays` when building `VisionComponent` to exactly `kVisionRayCapacity` for now.

  **Migration note**:
  - Any genomes with `sensory.vision_rays != kVisionRayCapacity` will have their ray count coerced at phenotype build time.
  - This intentionally prevents alternate vision layouts from shifting sensor indices while we introduce the social tail.

  **References**:
  - `sim/src/genetics/phenotype_builder.cpp` - sets `VisionComponent.ray_count`.

  **Acceptance Criteria**:
  - Test passes; no runtime changes in vision slot sizing.

- [ ] 12. Extract shared creature-behavior slice registration + use it in entrypoints/tests

  **Why**:
  - Prevent drift between `sim/src/main.cpp`, `sim/src/core/scenario.cpp`, and tests.

  **What to do**:
  - Add a helper that registers ONLY the shared behavior slice, after the systems it references exist:
    - Header: `sim/include/evolution/sim/system_slices.h`
    - Impl: `sim/src/core/system_slices.cpp`
    - Function: `void RegisterCreatureBehaviorSlice(Scheduler&, genetics::GenomeStorage&, IPhysicsBackend&);`
  - The helper registers this ordered slice:
    - `VisionSystem`
    - `CreatureSpatialIndexSystem`
    - `SocialBehaviorSystem`
    - `BrainInferenceSystem`
    - `MotorSystem`
    - `DecompositionSystem`
    - `MetabolismSystem`
  - Update both entrypoints to call this helper instead of manually registering these systems.
  - Ordering test strategy:
    - Build a `Scheduler`, register the slice via the helper, then assert order via `scheduler.system_names()`.
    - Provide a valid physics backend for `VisionSystem` (use `SimplePhysicsBackend` like `sim/src/core/scenario.cpp`).

  **Acceptance Criteria**:
  - New ordering test asserts:
    - `VisionSystem` < `CreatureSpatialIndexSystem` < `SocialBehaviorSystem` < `BrainInferenceSystem` < `MotorSystem`.
  - `ctest --test-dir build` passes.

- [ ] 13. Documentation + contracts update

  **What to do**:
  - Update:
    - `documentation/architecture/overview.md` (new system + component list + ordering).
    - `documentation/data-contracts/inter_module_contracts.md` (social signal data flow; soil/decomposition loop).
    - Any module docs that mention decomposition/feeding/environment if they now depend on social signals.

  **Acceptance Criteria**:
  - Docs explicitly state:
    - where decomposition runs,
    - what social signals exist,
    - which systems read/write each new component,
    - determinism/perf guardrails.

---

## Self-Review / Gap Classification

### Defaults Applied (override if desired)
- Social neighbor radius / territory radius: choose a conservative constant default (e.g., 10m) and clamp.
- Territory center: default to creature spawn position (first observed `TransformComponent.position`).
- Pack hunting “target”: default to nearest herbivore (via spatial index) for carnivores.

### Potential Risks
- Determinism: unordered_map iteration order in `CreatureSpatialIndex` cells.
- Performance: O(N*k) neighbor aggregation if radius too large.
- Sensor mapping: ensuring appended sensors never shift existing indices.

---

## Success Criteria

### Verification Commands
- `cmake -S . -B build && cmake --build build`
- `ctest --test-dir build`

### Final Checklist
- [ ] Decomposition runs in both scenario and main entrypoints.
- [ ] Corpses reliably increase soil nutrients (test-proven).
- [ ] Social signals are computed deterministically and exposed as brain inputs (append-only).
- [ ] Flocking, territoriality, and pack hunting have at least one focused unit test each.
- [ ] Docs/contracts updated.
