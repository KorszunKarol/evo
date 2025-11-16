# Physics v1 Milestone

## Goal
Deliver a deterministic, backend-driven physics stack that supports colliders (sphere, AABB, upright capsule), contact response with friction and restitution, collision events, and a replaceable backend abstraction.

## Feature Checklist
- `ColliderComponent`, `PhysicsMaterial`, `CollisionFilter`, `RigidbodyComponent` ECS data
- Backend interface (`IPhysicsBackend`) with contact events and diagnostics
- CPU reference backend (`SimplePhysicsBackend`)
  - Uniform-grid spatial hash broad-phase
  - Analytic narrow-phase for sphere/AABB/capsuleY + ground plane
  - Sequential impulse solver (normal + Coulomb friction)
  - Baumgarte positional correction with configurable slop
  - Begin/stay/end contact events
- Orchestrator system (`PhysicsSystem`) replacing legacy integrator
- GoogleTest coverage for narrow-phase helpers and ground bounce regression
- Documentation updates (components, physics system) and new milestone doc

## Acceptance Criteria
- 1,000 dynamic spheres remain stable at 60 Hz with default settings on reference hardware
- Contact events are emitted deterministically across runs with identical seeds
- Ground restitution respects per-collider material settings (bounce regression test passes)
- Scheduler wiring uses `PhysicsSystem` with `SimplePhysicsBackend`
- All docs referencing `SimplePhysicsSystem` now describe the backend architecture
- `ctest` executes the new physics tests

## Implementation Summary
1. **ECS Extensions** – Added material/filter/collider/rigidbody components with doc updates.
2. **Backend Abstraction** – Introduced `IPhysicsBackend` interface and new `PhysicsSystem` façade.
3. **Simulation Pipeline** – Implemented broad-phase, narrow-phase, and solver modules with deterministic ordering.
4. **Simple Backend** – CPU backend performing sync → detect → solve → integrate → events.
5. **Tooling** – Updated CMake to compile physics modules and link GoogleTest; added regression tests.
6. **Documentation** – Refreshed physics module guide; authored this milestone summary.

## Follow-up Ideas
- Implement PhysX backend behind the same interface
- Add rotational dynamics and non-axis-aligned capsules/OBBs
- Introduce continuous collision detection for fast movers
- Surface contact events to gameplay/NPC systems via messaging bus
- Profile and SIMD-optimize the solver hot path
