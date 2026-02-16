# Module Ownership

This file defines ownership boundaries for stabilization and modularization work.

## Modules

### `sim/src/environment/*`
- Scope: Terrain, soil, plant lifecycle, feeding integration.
- Owner role: Environment maintainer.
- Change policy: Requires passing scenario and integration tests.

### `sim/src/physics/*` and `sim/src/physics_system.cpp`
- Scope: Collision broad phase, narrow phase, solver, backend integration.
- Owner role: Physics maintainer.
- Change policy: Must keep deterministic behavior under fixed timestep.

### `sim/src/genetics/*` and `sim/src/reproduction_system.cpp`
- Scope: Genome storage/ops, phenotype build, reproduction flows.
- Owner role: Genetics maintainer.
- Change policy: Must preserve deterministic seed behavior.

### `sim/src/telemetry/*` and `sim/src/stats_system.cpp`
- Scope: Runtime metrics/events and rollups.
- Owner role: Observability maintainer.
- Change policy: Telemetry must stay side-effect free with respect to simulation outcomes.

### `tests/*`
- Scope: Unit/integration/scenario/performance contracts.
- Owner role: Quality maintainer.
- Change policy: Failing tests block merge to integration branch.

### Build and workflow (`CMakeLists.txt`, `.github/workflows/*`, `tools/*`)
- Scope: Build graph, CI gates, project health tooling.
- Owner role: Build/release maintainer.
- Change policy: Build + full `ctest` gate required for merge.

## Escalation Rule
- Any cross-module refactor touching 3+ modules requires explicit reviewer acknowledgement from at least one owner role listed above.
