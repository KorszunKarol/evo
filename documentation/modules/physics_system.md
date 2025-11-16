# Module: Physics System

## Overview

The physics module now exposes a pluggable backend architecture. `PhysicsSystem` is an ECS system that delegates all simulation work to an `IPhysicsBackend` implementation. The default `SimplePhysicsBackend` performs deterministic CPU simulation using a uniform-grid broad-phase, analytic narrow-phase primitives, and a sequential impulse solver with Baumgarte positional correction. Contact events are emitted each tick so higher-level systems can react to collisions or trigger volumes.

## File Map

- `sim/include/evolution/sim/physics_system.h`
- `sim/src/physics_system.cpp`
- `sim/include/evolution/sim/physics/backend.h`
- `sim/include/evolution/sim/physics/simple_backend.h`
- `sim/include/evolution/sim/physics/broad_phase.h`
- `sim/include/evolution/sim/physics/narrow_phase.h`
- `sim/include/evolution/sim/physics/solver.h`
- `sim/include/evolution/sim/physics/physics_types.h`
- `sim/src/physics/simple_backend.cpp`
- `sim/src/physics/broad_phase.cpp`
- `sim/src/physics/narrow_phase.cpp`
- `sim/src/physics/solver.cpp`

## Key Types

### PhysicsSystem
- **Role**: ECS system façade.
- **Public API**:
  ```cpp
  explicit PhysicsSystem(std::unique_ptr<IPhysicsBackend>);
  void tick(SimulationContext& context) override;
  std::string_view name() const override;
  const IPhysicsBackend::Stats& stats() const;
  ```
- **Responsibilities**:
  - store backend ownership
  - call `sync_from_registry()` then `step()` each tick
  - cache backend statistics for diagnostics
- **Threading**: not thread-safe; invoke on simulation thread only.

### IPhysicsBackend (interface)
- Declared in `physics/backend.h`.
- Core methods:
  - `configure(const Config&)`
  - `sync_from_registry(entt::registry&)`
  - `step(entt::registry&, double dt)`
  - `raycast(origin, direction, max_distance)`
  - `contact_events()` → span of per-tick begin/stay/end events
  - `stats()` → diagnostic counters
- `Config` fields: spatial hash cell size, solver iterations, Baumgarte factor, penetration slop.
- `ContactEvent`: reports the colliding entities, contact normal/point, penetration depth, and begin/stay/end flags.

### SimplePhysicsBackend
- Deterministic CPU backend used by default.
- Configuration struct `SimplePhysicsConfig` extends backend `Config` with gravity and ground plane height.
- **Terrain Integration**: If `Terrain` service exists in `registry.ctx()`, ground collisions use heightfield queries instead of flat plane.
- Pipeline per tick:
  1. **Sync** – rebuild body records from registry, compute world AABBs, populate spatial hash.
  2. **Broad-phase** – generate candidate pairs from the hash.
  3. **Narrow-phase** – analytic intersections for sphere, AABB, and upright capsule, plus ground contacts (plane or heightfield).
  4. **Material Mixing** – friction geometric mean, restitution max.
  5. **Solver** – sequential impulses for normal + Coulomb friction, followed by Baumgarte positional correction.
  6. **Integration** – semi-implicit Euler integration, damping, force reset.
  7. **Event Dispatch** – compare current contacts with previous frame, emit begin/stay/end events.

### SpatialHash (broad phase)
- Uniform grid, deterministic ordering.
- Inserts world AABBs and builds candidate pairs with lexicographic sorting/deduplication.

### Narrow-phase
- Public helpers `collide_sphere_sphere`, `collide_sphere_aabb`, `collide_aabb_aabb`, `collide_sphere_capsule_y`, `collide_capsule_y_capsule_y`, `collide_with_ground`.
- `collide_with_ground` supports two modes:
  - **Plane mode**: Constant `ground_y` height (default fallback)
  - **Terrain mode**: Queries `Terrain*` from registry context for heightfield collision
- When terrain is present, uses `terrain->height(x, z)` and `terrain->normal(x, z)` for proper slope response.
- Return `ContactManifold` (up to 4 contact points, impulse caches for warm-starting).

### Solver
- `solve_velocity_constraints` – sequential impulses for normal and two tangential directions.
- `solve_position_constraints` – Baumgarte correction with configurable slop.
- Rotation is currently omitted; capsules are upright (Y-axis) only.

## Data Flow

```
PhysicsSystem::tick()
    ├─ backend->sync_from_registry(registry)
    │    ├─ rebuild body cache
    │    └─ populate spatial hash
    ├─ backend->step(registry, dt)
    │    ├─ broad-phase → candidate pairs
    │    ├─ narrow-phase → manifolds
    │    ├─ integrate forces (gravity + per-body forces)
    │    ├─ solve_velocity_constraints()
    │    ├─ integrate velocities/positions
    │    ├─ solve_position_constraints()
    │    └─ dispatch_contact_events()
    └─ stats_cache_ ← backend->stats()
```

## Components in Play

| Component | Usage |
|-----------|-------|
| `TransformComponent` | Current world position (updated after integration and correction). |
| `KinematicsComponent` | Linear velocity, accumulated forces, damping, restitution, friction. |
| `ColliderComponent` | Shape (sphere/AABB/capsuleY), local offset, material, collision filter. |
| `RigidbodyComponent` | Flags for static/kinematic bodies (disable impulse resolution). |

## Collision Filters & Triggers

- `CollisionFilter.category` & `mask` must overlap in both directions for the pair to be considered.
- `is_trigger = true` skips impulse solving but still emits contact events.

## Contact Events

Each contact event includes:
- entities A/B (B may be `entt::null` for ground plane)
- contact normal/point/penetration
- `begin`, `stay`, `end` booleans
Events persist until the next `step` call (span returned by `contact_events()`).

## Testing Strategy

- **Unit tests** (`tests/physics/test_narrow_phase.cpp`)
  - sphere–sphere normal/pentration
  - sphere–AABB overlap
- **Integration tests** (`tests/physics/test_simple_backend.cpp`)
  - sphere dropped onto ground rebounds with restitution=1.
- **Determinism / performance** (future)
  - hash-based pair generation stability
  - 1k-body perf budget (~16 ms / tick target)

## Terrain Integration

The physics backend automatically detects and uses terrain heightfields when available:

**Detection**:
- Checks `registry.ctx().find<Terrain>()` during narrow-phase
- If terrain exists: uses heightfield collision
- If terrain absent: falls back to flat `ground_height` plane

**Heightfield Collision**:
- Queries `terrain->height(x, z)` for ground elevation
- Uses `terrain->normal(x, z)` for proper slope response
- Applies friction and restitution along local surface normal
- Prevents tunneling on steep slopes

**Data Contract**:
- Physics reads: `registry.ctx<Terrain>()` (read-only)
- Terrain lifetime: Created by `EnvironmentBootstrapSystem`, persists for simulation
- Thread safety: Terrain queries are thread-safe (read-only)

## Future Extensions

- Angular dynamics & inertia tensors
- Continuous collision detection
- Additional shapes (OBB, mesh proxies)
- GPU offload for solver & broad-phase
- PhysX backend implementation honoring the same interface

## Related Docs

- [Components Module](./components.md) – collider/material/filter data definitions.
- [Environment Module](./environment.md) – terrain generation and services.
- [Physics V1 Notes](./physics_v1.md) – milestone scope and acceptance criteria.
- [Core Simulation Module](./core_simulation.md) – scheduler/timing infrastructure.

