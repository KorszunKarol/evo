# Module: Core Simulation

## Overview

The core simulation module provides the foundational infrastructure for the entire evolution system. It manages the simulation lifecycle, system execution, and provides the primary interfaces for interacting with the simulation.

## Files

- `sim/include/evolution/sim/simulation_app.h`
- `sim/src/simulation_app.cpp`
- `sim/include/evolution/sim/scheduler.h`
- `sim/src/scheduler.cpp`
- `sim/include/evolution/sim/simulation_context.h`
- `sim/include/evolution/sim/metabolism_system.h`
- `sim/src/metabolism_system.cpp`
- `sim/include/evolution/sim/fitness_update_system.h`
- `sim/src/fitness_update_system.cpp`
- `sim/include/evolution/sim/stats_system.h`
- `sim/src/stats_system.cpp`
- `sim/include/evolution/sim/brain_inference_system.h`
- `sim/src/brain_inference_system.cpp`
- `sim/include/evolution/sim/motor_system.h`
- `sim/src/motor_system.cpp`

## Classes

### SimulationApp

**Purpose**: High-level façade managing simulation lifecycle and systems.

**Responsibilities**:
- Owns the ECS registry
- Manages simulation time progression
- Orchestrates system execution
- Provides access to simulation state

**Public API**:

```cpp
// Construction
explicit SimulationApp(SimulationConfig config = {});

// Execution
void tick();                                    // Single timestep
void run_for_steps(std::size_t steps);         // Multiple timesteps

// Accessors
[[nodiscard]] entt::registry& registry();     // Mutable registry access
[[nodiscard]] Scheduler& scheduler();          // System management
[[nodiscard]] double simulation_time() const;  // Current sim time
[[nodiscard]] double fixed_dt() const;        // Timestep duration
```

**Parameters**:

- `SimulationConfig config`:
  - `double fixed_dt`: Fixed timestep in seconds (default: 1/60.0)
  - Used during construction only
  - No validation performed (caller responsible for positive values)

**Returns**:

- `registry()`: Reference to internal `entt::registry` instance
  - Lifetime: Valid for lifetime of `SimulationApp`
  - Thread safety: Not thread-safe
  - Modifications: Direct component access allowed

- `scheduler()`: Reference to internal `Scheduler` instance
  - Lifetime: Valid for lifetime of `SimulationApp`
  - Thread safety: Not thread-safe
  - Usage: Register systems during setup phase

- `simulation_time()`: Current accumulated simulation time
  - Type: `double` (seconds)
  - Range: [0, ∞)
  - Precision: Double precision floating point
  - Updates: Incremented by `fixed_dt` each tick

- `fixed_dt()`: Configured timestep duration
  - Type: `double` (seconds)
  - Immutable: Set at construction, never changes
  - Typical values: 1/60.0 (60 Hz), 1/120.0 (120 Hz)

**State Management**:

- `registry_`: Owned `entt::registry` instance
  - Lifetime: Created at construction, destroyed at destruction
  - Thread safety: Not thread-safe
  - Access: Via `registry()` accessor

- `scheduler_`: Owned `Scheduler` instance
  - Lifetime: Created at construction, destroyed at destruction
  - Thread safety: Not thread-safe
  - Access: Via `scheduler()` accessor

- `fixed_dt_`: Timestep duration
  - Set from `config.fixed_dt` at construction
  - Never modified after construction

- `simulation_time_`: Accumulated time
  - Initialized to 0.0
  - Incremented by `fixed_dt_` each `end_tick()`

- `tick_count_`: Total ticks executed
  - Initialized to 0
  - Incremented each `end_tick()`

**Lifecycle**:

1. **Construction**: Initialize registry, scheduler, time counters
2. **Setup Phase**: Register systems via `scheduler().add_system()`
3. **Execution Phase**: Call `tick()` or `run_for_steps()` repeatedly
4. **Destruction**: Automatic cleanup of all owned resources

**Thread Safety**:

- **Not thread-safe**: All methods must be called from single thread
- **Exception**: `fixed_dt()` is read-only and safe for concurrent reads
- **Future**: Parallel tick execution planned for later milestones

**Error Handling**:

- No exceptions thrown by `SimulationApp` itself
- Systems may throw exceptions (propagate to caller)
- Registry operations may throw (EnTT exceptions)

**Performance**:

- `tick()`: O(S) where S = number of registered systems
- `run_for_steps(n)`: O(S × n)
- Memory: O(E × C) where E = entities, C = average components

### Scheduler

**Purpose**: Manages ordered execution of registered systems.

**Responsibilities**:
- Store systems in stage + registration order
- Execute systems sequentially each tick
- Provide system management interface

**Public API**:

```cpp
enum class SystemStage : std::size_t {
    Bootstrap,
    PrePhysics,
    Ecology,
    Physics,
    Metrics,
    PostTick,
    Count
};

void add_system(std::unique_ptr<ISystem> system);
void add_system(SystemStage stage, std::unique_ptr<ISystem> system);
void tick_systems(SimulationContext& context);
```

**Parameters**:

- `add_system(std::unique_ptr<ISystem> system)`:
  - **Input**: Ownership-transferring pointer to system instance
  - **Precondition**: `system != nullptr`
  - **Postcondition**: System added to default `Ecology` stage
  - **Order**: Stage order then registration order
  - **Thread safety**: Must be called during setup phase (single-threaded)

- `add_system(SystemStage stage, std::unique_ptr<ISystem> system)`:
  - **Input**: stage bucket + ownership-transferring system pointer
  - **Precondition**: `system != nullptr`
  - **Postcondition**: System added to selected stage
  - **Order**: Stages execute in fixed order (`Bootstrap` → `PrePhysics` → `Ecology` → `Physics` → `Metrics` → `PostTick`)
  - **Thread safety**: Must be called during setup phase (single-threaded)

- `tick_systems(SimulationContext& context)`:
  - **Input**: Reference to tick-scoped context
  - **Precondition**: Context must reference valid registry
  - **Postcondition**: All systems executed once
  - **Order**: Sequential execution (no parallelism)
  - **Thread safety**: Must be called from simulation thread

**Returns**:

- `add_system()`: `void` (no return value)
- `tick_systems()`: `void` (no return value)

**State Management**:

- `staged_systems_`: stage-indexed vectors of `std::unique_ptr<ISystem>`
  - Storage: Owns all registered systems
  - Lifetime: Systems destroyed when scheduler destroyed
  - Order: Preserves insertion order within each stage
  - Capacity: Grows dynamically as systems added

**Execution Model**:

```
For each stage in fixed stage order:
    For each system in stage:
        1. Log system name (trace level)
        2. Call system->tick(context)
        3. Continue to next system
```

**Thread Safety**:

- **Not thread-safe**: All methods require single-threaded access
- **Setup phase**: `add_system()` called during initialization
- **Execution phase**: `tick_systems()` called from simulation thread

**Performance**:

- `add_system()`: O(1) amortized (vector push_back)
- `tick_systems()`: O(S) where S = number of systems
- Memory: O(S) for system storage

### SimulationContext

**Purpose**: Bundles state passed to systems during a simulation tick.

**Responsibilities**:
- Provide read/write access to ECS registry
- Expose immutable timing metadata
- Scope tick-specific information

**Public API**:

```cpp
// Construction
SimulationContext(entt::registry& registry, double fixed_dt, double sim_time);

// Accessors
[[nodiscard]] entt::registry& registry();
[[nodiscard]] const entt::registry& registry() const;
[[nodiscard]] double fixed_dt() const;
[[nodiscard]] double simulation_time() const;
```

**Parameters**:

- Constructor:
  - `entt::registry& registry`: Reference to ECS registry (must outlive context)
  - `double fixed_dt`: Timestep duration in seconds
  - `double sim_time`: Current simulation time in seconds

**Returns**:

- `registry()`: Mutable registry reference
  - Lifetime: Valid while context exists
  - Thread safety: Not thread-safe
  - Usage: Systems modify components via this reference

- `registry() const`: Immutable registry reference
  - Lifetime: Valid while context exists
  - Thread safety: Read-only access (but registry itself not thread-safe)
  - Usage: Systems read components via this reference

- `fixed_dt()`: Timestep duration
  - Type: `double` (seconds)
  - Immutable: Never changes during context lifetime
  - Usage: Systems use for time-based calculations

- `simulation_time()`: Current simulation time
  - Type: `double` (seconds)
  - Immutable: Never changes during context lifetime
  - Usage: Systems can timestamp events

**Lifetime**:

- Created: At start of each `SimulationApp::tick()`
- Destroyed: At end of `SimulationApp::tick()`
- Scope: Single tick execution only

**Thread Safety**:

- **Not thread-safe**: Context tied to single tick execution
- **Registry access**: Not thread-safe (shared with other systems)
- **Timing data**: Read-only, safe for concurrent reads (but context not shared)

**Usage Pattern**:

```cpp
void MySystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const double dt = context.fixed_dt();
    const double time = context.simulation_time();
    
    // Use registry and timing info
}
```

### MetabolismSystem

**Purpose**: Provide the baseline life-support loop by draining metabolic energy and pruning depleted entities.

**Responsibilities**:
- Iterate all entities with `MetabolismComponent`
- Consume `basal_rate * dt` from `energy`
- Clamp `energy` to `[0, max_energy]`
- Destroy entities whose reserves reach zero (configurable)

**Public API**:

```cpp
explicit MetabolismSystem(bool destroy_on_zero = true) noexcept;
void tick(SimulationContext& context) override;
[[nodiscard]] std::string_view name() const override noexcept;
void set_destroy_on_zero(bool enabled) noexcept;
```

**Parameters**:

- `destroy_on_zero` (constructor):
  - Type: `bool`
  - Meaning: Whether the system destroys entities when energy reaches zero
  - Default: `true`
  - Validation: None

- `context` (`tick`):
  - Type: `SimulationContext&`
  - Provides registry/timing data
  - Precondition: Context references a live registry

- `enabled` (`set_destroy_on_zero`):
  - Type: `bool`
  - Meaning: Toggle for depletion-driven destruction

**Returns**:

- `name()` → `std::string_view` literal `"metabolism"`
- Other functions return `void`

**State Management**:

- `destroy_on_zero_`: Current depletion policy
- `recycle_bin_`: Scratch buffer holding depleted entities until destruction phase

**Thread Safety**:

- `tick()` mutates ECS state → not thread-safe
- `set_destroy_on_zero()` should be invoked during setup
- `name()` is safe for concurrent reads

**Performance**:

- `tick()` is O(N) with N = entities containing `MetabolismComponent`
- Memory overhead: transient vector sized to worst-case depletion count

### FitnessUpdateSystem

**Purpose**: Integrate age and energy metrics into a scalar fitness score for deterministic selection.

**Responsibilities**:
- Iterate all entities possessing both `FitnessComponent` and `MetabolismComponent`
- Increment `FitnessComponent::age_seconds` by the fixed timestep
- Accumulate `energy * dt` into `FitnessComponent::energy_int_accum`
- Recompute `FitnessComponent::last_fitness` using configured weights

**Public API**:

```cpp
explicit FitnessUpdateSystem(FitnessWeights weights = {}) noexcept;
void tick(SimulationContext& context) override;
[[nodiscard]] std::string_view name() const noexcept override;
```

**Parameters**:

- `weights` (constructor):
  - Type: `FitnessWeights`
  - Meaning: Weights applied to age, energy integral, and offspring count
  - Default: `{0.2, 1.0, 5.0}`

- `context` (`tick`):
  - Type: `SimulationContext&`
  - Provides registry and timing data

**Returns**:

- `name()` → `std::string_view` literal `"fitness_update"`
- Other functions return `void`

**State Management**:

- `weights_`: Immutable coefficients applied during fitness recompute

**Execution Order**:

- Should run after cleanup systems to avoid counting destroyed entities
- Must precede species indexing, reproduction, and selection systems

**Performance**:

- `tick()` is O(N) with N = entities matching `{FitnessComponent, MetabolismComponent}`
- No dynamic allocations

### StatsSystem

**Purpose**: Emit periodic health metrics (entity counts, mean metabolic energy) for headless diagnostics.

**Responsibilities**:
- Track elapsed time against a reporting interval
- Sample registry metrics when interval elapses
- Log values through `spdlog`

**Public API**:

```cpp
explicit StatsSystem(double interval_seconds = 1.0) noexcept;
void tick(SimulationContext& context) override;
[[nodiscard]] std::string_view name() const override noexcept;
void set_interval(double interval_seconds) noexcept;
```

**Parameters**:

- `interval_seconds` (constructor / setter):
  - Type: `double`
  - Meaning: Seconds between logs (`<= 0` → every tick)
  - Default: `1.0`
  - Validation: None

- `context` (`tick`):
  - Type: `SimulationContext&`
  - Provides registry/timing metadata

**Returns**:

- `name()` → `std::string_view` literal `"stats"`
- `tick()`/`set_interval()` return `void`

**State Management**:

- `interval_`: Current logging cadence in seconds
- `accumulator_`: Accumulated elapsed time since last log

**Thread Safety**:

- `tick()` reads registry → must stay on simulation thread
- `set_interval()` should be invoked during setup
- `name()` safe for concurrent reads

**Performance**:

- `tick()` is O(M) with M = entities carrying `MetabolismComponent`
- Logging cost dominated by `spdlog::info`

### BrainInferenceSystem

**Purpose**: Executes neural controllers (MLP or NEAT) and writes actuation commands.

**Responsibilities**:
- Iterate entities with `BrainComponent` and `ActuationComponent`
- Respect `update_rate_hz` (accumulate dt, skip frames when needed)
- Dispatch to MLP or NEAT inference engines
- Write normalized outputs to `ActuationComponent`

**Public API**:

```cpp
explicit BrainInferenceSystem(genetics::GenomeStorage& storage) noexcept;
void tick(SimulationContext& context) override;
[[nodiscard]] std::string_view name() const override noexcept;
```

**Parameters**:

- `storage` (constructor):
  - Type: `genetics::GenomeStorage&`
  - Meaning: Source of serialized genomes (must outlive system)
  - Validation: None

- `context` (`tick`):
  - Type: `SimulationContext&`
  - Provides registry/timing metadata

**Returns**:

- `name()` → `std::string_view` literal `"brain_inference"`
- `tick()` returns `void`

**State Management**:

- `storage_`: Reference to genome storage
- `neat_slots_`: Cached NEAT runtime instances (per-genome)
- `input_buffer_`: Reusable input buffer for sensor values
- `output_buffer_`: Reusable output buffer for brain outputs

**Thread Safety**:

- `tick()` mutates ECS state → not thread-safe
- Constructor stores reference (must outlive system)
- `name()` safe for concurrent reads

**Performance**:

- `tick()` is O(E × W) where E = active brains, W = weights/edges per brain
- Update rate control skips evaluation when `accum < update_interval` (saves CPU)

### MotorSystem

**Purpose**: Converts brain actuation commands into physics impulses and charges metabolic cost.

**Responsibilities**:
- Read `ActuationComponent` commands
- Apply forces to `KinematicsComponent.accumulated_force`
- Deduct energy cost from `MetabolismComponent`
- Reset actuation commands after application

**Public API**:

```cpp
explicit MotorSystem(double impulse_scale = 150.0, double jump_impulse = 250.0) noexcept;
void tick(SimulationContext& context) override;
[[nodiscard]] std::string_view name() const override noexcept;
```

**Parameters**:

- `impulse_scale` (constructor):
  - Type: `double`
  - Meaning: Baseline Newton force applied for unit actuation
  - Default: `150.0`

- `jump_impulse` (constructor):
  - Type: `double`
  - Meaning: Vertical impulse magnitude for jump command
  - Default: `250.0`

- `context` (`tick`):
  - Type: `SimulationContext&`
  - Provides registry and timestep

**Returns**:

- `name()` → `std::string_view` literal `"motor"`
- `tick()` returns `void`

**State Management**:

- `impulse_scale_`: Force scaling coefficient
- `jump_impulse_`: Jump impulse magnitude

**Thread Safety**:

- `tick()` mutates ECS state → not thread-safe
- Constructor parameters immutable after construction
- `name()` safe for concurrent reads

**Performance**:

- `tick()` is O(N) where N = entities with `ActuationComponent`
- Force application is O(1) per entity

## Data Contracts

### SimulationApp ↔ Scheduler

**Contract**: `SimulationApp` owns `Scheduler`, provides access via `scheduler()`.

**Data Flow**:
- Setup: `SimulationApp` → `Scheduler::add_system()` (system registration)
- Execution: `SimulationApp::tick()` → `Scheduler::tick_systems()` (system execution)

**Ownership**: `SimulationApp` owns `Scheduler` instance

**Threading**: Both accessed from single simulation thread

### SimulationApp ↔ SimulationContext

**Contract**: `SimulationApp` creates `SimulationContext` each tick, passes to scheduler.

**Data Flow**:
- Creation: `SimulationApp::tick()` creates context with registry reference
- Propagation: Context passed to `Scheduler::tick_systems()`
- Distribution: Context passed to each `ISystem::tick()`

**Lifetime**: Context created at tick start, destroyed at tick end

**Registry Reference**: Context holds reference to `SimulationApp`'s registry

### Scheduler ↔ ISystem

**Contract**: `Scheduler` owns system instances, invokes `tick()` method.

**Data Flow**:
- Registration: System passed to `Scheduler::add_system()` (ownership transfer)
- Optional stage routing: `Scheduler::add_system(SystemStage, ...)`
- Execution: `Scheduler::tick_systems()` calls `system->tick(context)`

**Ownership**: `Scheduler` owns all registered systems via `unique_ptr`

**Interface**: Systems implement `ISystem` interface

### Systems ↔ Registry

**Contract**: Systems access registry via `SimulationContext::registry()`.

**Data Flow**:
- Read: Systems query components via `registry.view<Components>()`
- Write: Systems modify components via component references

**Access Pattern**: Systems use EnTT views for component iteration

**Thread Safety**: Registry access not thread-safe (single-threaded execution)

### MetabolismSystem ↔ MetabolismComponent

**Contract**: MetabolismSystem reads and writes `MetabolismComponent` fields and may destroy entities.

- **Read**: `energy`, `max_energy`, `basal_rate`
- **Write**: `energy`
- **Side Effects**: Optional `registry.destroy(entity)` when energy depleted
- **Guarantees**: `energy` clamped to `[0, max_energy]` after each tick

### StatsSystem ↔ Registry

**Contract**: StatsSystem samples registry-level statistics and aggregates energy metrics.

- **Read**: `registry.alive()`, `MetabolismComponent::energy`
- **Write**: None (read-only)
- **Guarantees**: Logging skipped if no metabolic data is present

### BrainInferenceSystem ↔ GenomeStorage

**Contract**: BrainInferenceSystem reads genomes from storage to evaluate brains.

- **Read**: `storage.get(genome_id)` → `const Genome*`
- **Write**: None (read-only)
- **Guarantees**: Genome pointer valid during evaluation

### BrainInferenceSystem ↔ ActuationComponent

**Contract**: BrainInferenceSystem writes actuation commands based on brain outputs.

- **Read**: `BrainComponent` (update rate, accumulator)
- **Write**: `ActuationComponent` (impulses, jump, eat)
- **Guarantees**: Outputs clamped to valid ranges, commands reset after MotorSystem consumes

### MotorSystem ↔ ActuationComponent

**Contract**: MotorSystem consumes actuation commands and applies forces.

- **Read**: `ActuationComponent` (impulses, jump, eat)
- **Write**: `KinematicsComponent.accumulated_force`, `MetabolismComponent.energy`
- **Side Effects**: Resets `ActuationComponent` to defaults after application
- **Guarantees**: Forces applied deterministically, energy costs deducted

## Dependencies

### Internal Dependencies

- `sim/components`: Component definitions
- `sim/systems`: System interfaces (`ISystem`)

### External Dependencies

- **EnTT**: ECS framework (`entt::registry`)
- **spdlog**: Logging (`spdlog::trace`, `spdlog::info`)
- **genetics**: Genome storage and brain inference (via `sim_genetics` library)

## Extension Points

### Adding Custom Systems

1. Implement `ISystem` interface
2. Register with `app.scheduler().add_system(std::make_unique<MySystem>())`
3. Access components via `context.registry()`

### Custom Tick Hooks

Override `SimulationApp::begin_tick()` and `end_tick()` (currently private, could be made virtual)

## Performance Considerations

- **Registry access**: EnTT optimized for cache locality
- **System execution**: Sequential (future: parallel execution)
- **Context creation**: Minimal overhead (just reference storage)

## Testing Strategy

- **Unit tests**: Test `Scheduler` with mock systems
- **Integration tests**: Test `SimulationApp` with real systems
- **Performance tests**: Measure tick time with varying entity counts
