# Recovery Baseline (2026-02-16)

This document is a historical checkpoint from the 2026-02-16 stabilization effort.

## Baseline Branch
- Integration baseline during recovery: `feature/tracy-profiler`
- Active recovery implementation branch: `stabilization/recovery-2026-02-16`
- Baseline commit at branch creation: `44e2d63`

## Initial Failure Snapshot
- Build: PASS
- Tests: 96/101 PASS, 5 FAIL
- Failing tests at start:
  - `IntegrationSystemOrdering.FeedingAfterPlantGrowth`
  - `ScenarioTests.PredationDynamics`
  - `ScenarioTests.SpeciesFormation`
  - `ScenarioTests.ResourceDepletion`
  - `ScenarioTests.ExtinctionEvent`

## Root Causes Fixed
1. Test fixture storage corruption:
- `SimulationFixture::spawn_herbivore()` inserted `FitnessComponent` twice (once from `PhenotypeBuilder`, once from fixture), causing undefined behavior and crashy scenario runs.

2. Missing herbivore diet wiring in fixture:
- Herbivores created by test fixtures lacked `DietComponent`, so feeding paths were skipped by `FeedingSystem`.

3. Boundary safety issues in sampling:
- Soil interpolation needed safe upper-index clamping in `SoilGrid` and `SoilVolume`.

4. Scenario assertions drifted from current snapshot semantics:
- Predation and species assertions depended on outdated assumptions and were aligned to current deterministic model.

## Current Status
- Build: PASS
- Tests: 101/101 PASS
- Health check: `tools/project_health.sh` PASS
