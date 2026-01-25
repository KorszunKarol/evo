# Draft: C++ Simulation System Code Review

## Requirements (confirmed)
- Goal: Review existing C++ simulation code for non-optimal patterns and poorly written code; uphold highest coding standards.
- Scope: “All of them” (entire C++ codebase is in scope), but review will be a representative sample of files to gauge overall practices.
- Priorities: Performance + safety/correctness + general C++ best practices.
- Deliverable: Findings report + remediation work plan.

## Technical Decisions
- Review mode: Sampled review (not exhaustive); focus on hot-path costs, UB/race hazards, and consistent modern C++ style.

## Research Findings
- Core sim code appears under `sim/src/` (core, systems, physics, genetics, environment, telemetry).
- Tick loop: `sim/src/core/simulation_app.cpp` -> `sim/src/core/scheduler.cpp` (sequential system tick).
- Logging in hot paths: `sim/src/core/scheduler.cpp` logs each system per tick; `sim/src/core/simulation_app.cpp` logs begin/end tick; `sim/src/core/multi_rate_scheduler.cpp` logs on hourly/daily boundaries.
- Allocation-heavy genetics ops: `sim/src/genetics/genome_ops.cpp` performs deep clones via many `std::make_unique` calls (heap churn risk if on hot paths).
- Hash containers in algorithmic code: `sim/src/genetics/genome_ops.cpp`, `sim/src/physics/simple_backend.cpp`, `sim/src/systems/species_index_system.cpp` use `std::unordered_map/set` (potential perf + determinism iteration-order concerns).
- Tests: GoogleTest exists under `tests/` (unit/integration + determinism/perf-style tests). Tooling gaps likely include clang-format/clang-tidy configs, sanitizers, CI automation (to confirm with deeper scan).
- Brain inference likely alloc/fill heavy: `sim/src/systems/brain_inference_system.cpp` uses `input_buffer_.assign(sensor_count, 0.0)` in the per-entity update path.
- Fixed-point division behavior is ambiguous: `sim/include/evolution/sim/math/fixed_point.h` returns 0 on divide-by-zero with TODO-like comment (silent corruption risk).
- Broad-phase spatial hash: `sim/src/physics/broad_phase.cpp` inserts into `cells_` per occupied cell, then sorts/dedups occupants and later sorts/dedups candidate pairs (determinism-friendly but can be expensive; also potential coordinate range/overflow concerns).

### Tooling / Quality Gates (current state)
- Repo appears to have CMake warnings enabled (`-Wall -Wextra -Wpedantic`) in `CMakeLists.txt`, plus a simple `build.sh`.
- No `.clang-format` / `.clang-tidy` found; no sanitizer flags found in CMake; no CI config detected.

### Hot-Path Performance Anti-Patterns (examples to address)
- Tick loop logging: `sim/src/core/simulation_app.cpp` (trace per tick), `sim/src/core/scheduler.cpp` (trace per system per tick).
- Per-tick rebuilds / churn:
  - `sim/src/environment/feeding_system.cpp` creates and rebuilds `CreatureSpatialIndex` each tick.
  - `sim/src/physics/simple_backend.cpp` rebuilds physics records / clears multiple containers per tick (see hot-path scan).
  - `sim/src/systems/creature_spatial_index_system.cpp` rebuilds every tick.
- Per-entity buffer churn: `sim/src/systems/brain_inference_system.cpp` uses repeated `assign`/`resize` on buffers in the entity loop.
- Multi-pass registry scans + logging: `sim/src/systems/stats_system.cpp` performs multiple full passes and logs a large formatted message.
- Repeated neighbor queries + sorts: `sim/src/systems/social_behavior_system.cpp` clears/rebuilds neighbor lists several times per entity (with sorting each time).

### Safety / Correctness Risk Areas (examples to harden)
- Divide-by-zero semantics: `sim/include/evolution/sim/math/fixed_point.h` returns 0 for `/0` (risk: silent invalid state).
- Binary persistence: `sim/src/genetics/innovation_database.cpp` writes raw integers via `reinterpret_cast` without explicit endianness/compat contract.
- Broad-phase coordinate math: `sim/src/physics/broad_phase.cpp` uses `int` cell indices derived from `floor(min_value * inv_cell)`; verify range/clamping and overflow behavior.
- Widespread `noexcept` on functions that may allocate (e.g., deep clone/mutation paths): confirm that `noexcept` is accurate or accept `std::terminate` risk on allocation/throws.

### External References (to ground “highest standards”)
- C++ Core Guidelines (safety/resource/performance): https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- Clang sanitizers: https://clang.llvm.org/docs/AddressSanitizer.html ; https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html ; https://clang.llvm.org/docs/ThreadSanitizer.html
- Clang-tidy checks (overview): https://clang.llvm.org/extra/clang-tidy/
- Widespread `noexcept` in genetics code (e.g., `sim/src/genetics/genome_ops.cpp`): verify these functions do not allocate/throw, otherwise `noexcept` implies `std::terminate` on exceptions (including `bad_alloc`).
- Binary I/O uses `reinterpret_cast` in `sim/src/genetics/innovation_database.cpp` (save/load): verify portability assumptions (endianness/versioning/layout) and error handling.

## Open Questions
- What kind of simulation is this (physics, robotics, discrete-event, etc.) and what are the performance targets (Hz, typical entity counts)?
- Tooling constraints: can we require C++20/23, clang-format/clang-tidy, sanitizers, and CI checks as mandatory gates?
- Output preference: findings report only vs findings + remediation work plan.

## Scope Boundaries
- INCLUDE: Entire C++ codebase (sampled review; not exhaustive).
- EXCLUDE: None explicitly; will prioritize simulation/hot-path code if identifiable.
