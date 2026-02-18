# Pre-Cleanup Health Snapshot

- Date (UTC): 2026-02-18T11:10:41Z
- Branch: cleanup/windows-baseline-20260218
- Commit: b9fa8da

## Git Status
```text
## cleanup/windows-baseline-20260218
 M CMakeLists.txt
 M client/src/main.cpp
 M documentation/modules/render_client.md
 M sim/include/evolution/sim/components.h
 M sim/include/evolution/sim/environment/environment.h
 M sim/include/evolution/sim/environment/environment_bootstrap.h
 M sim/include/evolution/sim/environment/feeding_system.h
 M sim/include/evolution/sim/environment/plant_systems.h
 M sim/include/evolution/sim/environment/soil_volume.h
 M sim/include/evolution/sim/motor_system.h
 M sim/include/evolution/sim/physics/backend.h
 M sim/include/evolution/sim/scenario.h
 M sim/include/evolution/sim/species_index_system.h
 M sim/include/evolution/sim/stats_system.h
 M sim/include/evolution/sim/telemetry_system.h
 M sim/src/brain_inference_system.cpp
 M sim/src/environment/environment.cpp
 M sim/src/environment/environment_bootstrap.cpp
 M sim/src/environment/feeding_system.cpp
 M sim/src/environment/plant_systems.cpp
 M sim/src/environment/soil_system.cpp
 M sim/src/genetics/brain_mlp.cpp
 M sim/src/genetics/brain_neat.cpp
 M sim/src/genetics/phenotype_builder.cpp
 M sim/src/main.cpp
 M sim/src/metabolism_system.cpp
 M sim/src/motor_system.cpp
 M sim/src/physics/narrow_phase.cpp
 M sim/src/physics/simple_backend.cpp
 M sim/src/physics_system.cpp
 M sim/src/reproduction_system.cpp
 M sim/src/scenario.cpp
 M sim/src/species_index_system.cpp
 M sim/src/stats_system.cpp
 M sim/src/telemetry/telemetry_system.cpp
 M tests/sim/test_fixtures.cpp
 M tests/sim/test_telemetry.cpp
?? .clangd
?? AGENTS.md
?? client/include/
?? client/src/renderer/
?? docs/NEXT_FEATURES_PLAN.md
?? documentation/tools/health/
?? documentation/tools/tracy_profiler.md
?? scripts/
?? sim/include/evolution/sim/adaptive_control_system.h
?? sim/include/evolution/sim/environment/creature_spatial_index.h
?? sim/include/evolution/sim/perception/
?? sim/include/evolution/sim/population_monitor.h
?? sim/src/adaptive_control_system.cpp
?? sim/src/environment/creature_spatial_index.cpp
?? sim/src/perception/
?? sim/src/population_monitor.cpp
?? tests/sim/test_adaptive_control.cpp
?? tests/sim/test_advanced_predation.cpp
?? tests/sim/test_contact_sense.cpp
?? tests/sim/test_creature_spatial_index.cpp
?? tests/sim/test_density_reproduction.cpp
?? tests/sim/test_environment_bootstrap_regression.cpp
?? tests/sim/test_physics_world_bounds.cpp
?? tests/sim/test_population_dynamics_e2e.cpp
?? tests/sim/test_population_monitor.cpp
?? tests/sim/test_population_safety.cpp
?? tests/sim/test_scaling_controls.cpp
?? tests/sim/test_scenario_bootstrap_feeding.cpp
?? tests/sim/test_vision_system.cpp
?? tools/csvexport/
?? tools/run_render_client_wsl.sh
?? tools/run_stability_matrix.sh
?? tools/tracy_export.sh
```

## Diff Stat
```text
 CMakeLists.txt                                     |  44 +-
 client/src/main.cpp                                | 819 +--------------------
 documentation/modules/render_client.md             |  44 +-
 sim/include/evolution/sim/components.h             |  98 +++
 .../evolution/sim/environment/environment.h        |  26 +-
 .../sim/environment/environment_bootstrap.h        |  19 +-
 .../evolution/sim/environment/feeding_system.h     |  23 +-
 .../evolution/sim/environment/plant_systems.h      |  15 +-
 .../evolution/sim/environment/soil_volume.h        |  25 +-
 sim/include/evolution/sim/motor_system.h           |   7 +-
 sim/include/evolution/sim/physics/backend.h        |   2 +-
 sim/include/evolution/sim/scenario.h               |  22 +-
 sim/include/evolution/sim/species_index_system.h   |   8 +-
 sim/include/evolution/sim/stats_system.h           |   8 +-
 sim/include/evolution/sim/telemetry_system.h       |  39 +
 sim/src/brain_inference_system.cpp                 | 177 ++++-
 sim/src/environment/environment.cpp                |  17 +-
 sim/src/environment/environment_bootstrap.cpp      |  29 +-
 sim/src/environment/feeding_system.cpp             | 387 ++++++++--
 sim/src/environment/plant_systems.cpp              | 112 ++-
 sim/src/environment/soil_system.cpp                |  26 +-
 sim/src/genetics/brain_mlp.cpp                     |  29 +-
 sim/src/genetics/brain_neat.cpp                    |  36 +-
 sim/src/genetics/phenotype_builder.cpp             |  74 +-
 sim/src/main.cpp                                   |  65 +-
 sim/src/metabolism_system.cpp                      |   6 +-
 sim/src/motor_system.cpp                           |  63 +-
 sim/src/physics/narrow_phase.cpp                   |   6 +-
 sim/src/physics/simple_backend.cpp                 |  10 +-
 sim/src/physics_system.cpp                         | 118 ++-
 sim/src/reproduction_system.cpp                    |  79 +-
 sim/src/scenario.cpp                               | 138 +++-
 sim/src/species_index_system.cpp                   |  19 +-
 sim/src/stats_system.cpp                           |  55 +-
 sim/src/telemetry/telemetry_system.cpp             |  87 ++-
 tests/sim/test_fixtures.cpp                        |   4 +
 tests/sim/test_telemetry.cpp                       |  40 +-
 37 files changed, 1694 insertions(+), 1082 deletions(-)
```

## Branch Audit (base: master)
```text
== Branch Audit (base: master) ==

branch                                        behind       ahead        head
------                                        ------       -----        ----
backup_recovery_dirty_20260218-111030         0            25           b9fa8da
cleanup/windows-baseline-20260218             0            25           b9fa8da
feature/cpp-sim-quality-hardening             8            12           00e787e
feature/environment-scaffolding               8            4            0dd0214
feature/plant-genetics-phase-1                0            8            dcf90ff
feature/predator-prey-ecosystem               8            5            8651c3a
feature/terrain-shading-upgrades              8            2            6cf97e4
feature/tracy-profiler                        0            18           44e2d63
salvage/stash_0_                              0            2            c27fec0
salvage/stash_1_                              8            14           5af1a48
stabilization/recovery-2026-02-16             0            25           b9fa8da

Suggested cleanup policy:
- delete branches with ahead=0 and behind=0 (exact duplicates)
- rebase branches with behind>0 and ahead>0
- archive stale branches via tag before delete
```

## Project Health
```text
== Project Health ==
date: 2026-02-18T11:10:41Z
branch: cleanup/windows-baseline-20260218
commit: b9fa8da

== Working Tree ==
## cleanup/windows-baseline-20260218
 M CMakeLists.txt
 M client/src/main.cpp
 M documentation/modules/render_client.md
 M sim/include/evolution/sim/components.h
 M sim/include/evolution/sim/environment/environment.h
 M sim/include/evolution/sim/environment/environment_bootstrap.h
 M sim/include/evolution/sim/environment/feeding_system.h
 M sim/include/evolution/sim/environment/plant_systems.h
 M sim/include/evolution/sim/environment/soil_volume.h
 M sim/include/evolution/sim/motor_system.h
 M sim/include/evolution/sim/physics/backend.h
 M sim/include/evolution/sim/scenario.h
 M sim/include/evolution/sim/species_index_system.h
 M sim/include/evolution/sim/stats_system.h
 M sim/include/evolution/sim/telemetry_system.h
 M sim/src/brain_inference_system.cpp
 M sim/src/environment/environment.cpp
 M sim/src/environment/environment_bootstrap.cpp
 M sim/src/environment/feeding_system.cpp
 M sim/src/environment/plant_systems.cpp
 M sim/src/environment/soil_system.cpp
 M sim/src/genetics/brain_mlp.cpp
 M sim/src/genetics/brain_neat.cpp
 M sim/src/genetics/phenotype_builder.cpp
 M sim/src/main.cpp
 M sim/src/metabolism_system.cpp
 M sim/src/motor_system.cpp
 M sim/src/physics/narrow_phase.cpp
 M sim/src/physics/simple_backend.cpp
 M sim/src/physics_system.cpp
 M sim/src/reproduction_system.cpp
 M sim/src/scenario.cpp
 M sim/src/species_index_system.cpp
 M sim/src/stats_system.cpp
 M sim/src/telemetry/telemetry_system.cpp
 M tests/sim/test_fixtures.cpp
 M tests/sim/test_telemetry.cpp
?? .clangd
?? AGENTS.md
?? client/include/
?? client/src/renderer/
?? docs/NEXT_FEATURES_PLAN.md
?? documentation/tools/health/
?? documentation/tools/tracy_profiler.md
?? scripts/
?? sim/include/evolution/sim/adaptive_control_system.h
?? sim/include/evolution/sim/environment/creature_spatial_index.h
?? sim/include/evolution/sim/perception/
?? sim/include/evolution/sim/population_monitor.h
?? sim/src/adaptive_control_system.cpp
?? sim/src/environment/creature_spatial_index.cpp
?? sim/src/perception/
?? sim/src/population_monitor.cpp
?? tests/sim/test_adaptive_control.cpp
?? tests/sim/test_advanced_predation.cpp
?? tests/sim/test_contact_sense.cpp
?? tests/sim/test_creature_spatial_index.cpp
?? tests/sim/test_density_reproduction.cpp
?? tests/sim/test_environment_bootstrap_regression.cpp
?? tests/sim/test_physics_world_bounds.cpp
?? tests/sim/test_population_dynamics_e2e.cpp
?? tests/sim/test_population_monitor.cpp
?? tests/sim/test_population_safety.cpp
?? tests/sim/test_scaling_controls.cpp
?? tests/sim/test_scenario_bootstrap_feeding.cpp
?? tests/sim/test_vision_system.cpp
?? tools/csvexport/
?? tools/run_render_client_wsl.sh
?? tools/run_stability_matrix.sh
?? tools/tracy_export.sh

== Branch Divergence ==
origin/master...master: 0 8
master...backup_recovery_dirty_20260218-111030: 0 25
master...cleanup/windows-baseline-20260218: 0 25
master...feature/cpp-sim-quality-hardening: 8 12
master...feature/environment-scaffolding: 8 4
master...feature/plant-genetics-phase-1: 0 8
master...feature/predator-prey-ecosystem: 8 5
master...feature/terrain-shading-upgrades: 8 2
master...feature/tracy-profiler: 0 18
master...salvage/stash_0_: 0 2
master...salvage/stash_1_: 8 14
master...stabilization/recovery-2026-02-16: 0 25

Tip: run ./tools/branch_audit.sh master for detailed branch cleanup guidance.
== Stashes ==

Tip: run ./tools/stash_audit.sh for stash contents and salvage recommendations.

== Build + Tests ==
[build] cmake --build build
build: PASS
[test] ctest --test-dir build --output-on-failure
tests: PASS
```
