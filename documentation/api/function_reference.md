# API Reference: Complete Function Signatures

## SimulationApp

### Construction

```cpp
explicit SimulationApp(SimulationConfig config = {});
```

**Parameters**:
- `config`: `SimulationConfig` (optional, default-constructed)
  - `double fixed_dt`: Timestep duration in seconds (default: 1/60.0)

**Returns**: `SimulationApp` instance

**Exceptions**: None

**Complexity**: O(1)

---

### tick()

```cpp
void tick();
```

**Parameters**: None

**Returns**: `void`

**Exceptions**: May propagate exceptions from registered systems

**Complexity**: O(S) where S = number of registered systems

**Side Effects**:
- Advances simulation by one timestep
- Updates `simulation_time_` by `fixed_dt_`
- Increments `tick_count_`

---

### run_for_steps()

```cpp
void run_for_steps(std::size_t steps);
```

**Parameters**:
- `steps`: `std::size_t` - Number of ticks to execute
  - Range: `[1, SIZE_MAX]`
  - Precondition: `steps > 0` (undefined behavior if 0)

**Returns**: `void`

**Exceptions**: May propagate exceptions from registered systems

**Complexity**: O(S × steps) where S = number of registered systems

**Side Effects**: Calls `tick()` `steps` times

---

### registry()

```cpp
[[nodiscard]] entt::registry& registry();
```

**Parameters**: None

**Returns**: `entt::registry&` - Mutable reference to internal registry

**Exceptions**: None

**Complexity**: O(1)

**Thread Safety**: Not thread-safe

**Lifetime**: Valid for lifetime of `SimulationApp`

---

### scheduler()

```cpp
[[nodiscard]] Scheduler& scheduler();
```

**Parameters**: None

**Returns**: `Scheduler&` - Reference to internal scheduler

**Exceptions**: None

**Complexity**: O(1)

**Thread Safety**: Not thread-safe

**Lifetime**: Valid for lifetime of `SimulationApp`

---

### simulation_time()

```cpp
[[nodiscard]] double simulation_time() const;
```

**Parameters**: None

**Returns**: `double` - Current simulation time in seconds
  - Range: `[0.0, ∞)`
  - Precision: Double precision floating point

**Exceptions**: None

**Complexity**: O(1)

**Thread Safety**: Not thread-safe (value may change)

---

### fixed_dt()

```cpp
[[nodiscard]] double fixed_dt() const;
```

**Parameters**: None

**Returns**: `double` - Fixed timestep duration in seconds
  - Immutable: Set at construction, never changes
  - Typical: `1/60.0` (60 Hz) or `1/120.0` (120 Hz)

**Exceptions**: None

**Complexity**: O(1)

**Thread Safety**: Thread-safe (read-only constant)

---

## Scheduler

### add_system()

```cpp
void add_system(std::unique_ptr<ISystem> system);
```

**Parameters**:
- `system`: `std::unique_ptr<ISystem>` - Ownership-transferring pointer
  - Precondition: `system != nullptr` (undefined behavior if null)
  - Ownership: Transferred to scheduler

**Returns**: `void`

**Exceptions**: May throw `std::bad_alloc` if vector reallocation fails

**Complexity**: O(1) amortized

**Side Effects**: Adds system to execution list (preserves order)

**Thread Safety**: Not thread-safe (setup phase only)

---

### tick_systems()

```cpp
void tick_systems(SimulationContext& context);
```

**Parameters**:
- `context`: `SimulationContext&` - Tick-scoped simulation state
  - Precondition: Context references valid registry
  - Lifetime: Must outlive method call

**Returns**: `void`

**Exceptions**: May propagate exceptions from system `tick()` methods

**Complexity**: O(N) where N = number of registered systems

**Side Effects**: Invokes `tick()` on all registered systems sequentially

**Thread Safety**: Not thread-safe (execution phase only)

---

## SimulationContext

### Constructor

```cpp
SimulationContext(entt::registry& registry, double fixed_dt, double sim_time);
```

**Parameters**:
- `registry`: `entt::registry&` - Reference to ECS registry
  - Lifetime: Must outlive context
- `fixed_dt`: `double` - Timestep duration in seconds
  - Range: `(0.0, ∞)`
- `sim_time`: `double` - Current simulation time in seconds
  - Range: `[0.0, ∞)`

**Returns**: `SimulationContext` instance

**Exceptions**: None

**Complexity**: O(1)

---

### registry() (mutable)

```cpp
[[nodiscard]] entt::registry& registry();
```

**Parameters**: None

**Returns**: `entt::registry&` - Mutable registry reference

**Exceptions**: None

**Complexity**: O(1)

**Thread Safety**: Not thread-safe

---

### registry() (const)

```cpp
[[nodiscard]] const entt::registry& registry() const;
```

**Parameters**: None

**Returns**: `const entt::registry&` - Immutable registry reference

**Exceptions**: None

**Complexity**: O(1)

**Thread Safety**: Read-only access (but registry itself not thread-safe)

---

### fixed_dt()

```cpp
[[nodiscard]] double fixed_dt() const;
```

**Parameters**: None

**Returns**: `double` - Timestep duration in seconds

**Exceptions**: None

**Complexity**: O(1)

---

### simulation_time()

```cpp
[[nodiscard]] double simulation_time() const;
```

**Parameters**: None

**Returns**: `double` - Current simulation time in seconds

**Exceptions**: None

**Complexity**: O(1)

---

## ISystem Interface

### tick()

```cpp
virtual void tick(SimulationContext& context) = 0;
```

**Parameters**:
- `context`: `SimulationContext&` - Tick-scoped simulation state

**Returns**: `void`

**Exceptions**: Implementation-defined

**Complexity**: Implementation-defined (typically O(N) where N = entities)

**Pure Virtual**: Must be implemented by derived classes

---

### name()

```cpp
virtual std::string_view name() const = 0;
```

**Parameters**: None

**Returns**: `std::string_view` - System name identifier

**Exceptions**: None

**Complexity**: O(1)

**Pure Virtual**: Must be implemented by derived classes

---

### Destructor

```cpp
virtual ~ISystem() = default;
```

**Parameters**: None

**Returns**: None

**Exceptions**: None

**Virtual**: Allows safe polymorphic deletion

---

## PhysicsSystem

### Constructor

```cpp
explicit PhysicsSystem(std::unique_ptr<IPhysicsBackend> backend);
```

**Parameters**:
- `backend`: `std::unique_ptr<IPhysicsBackend>` – transfer of ownership to the system

**Returns**: `PhysicsSystem`

**Exceptions**: `std::invalid_argument` if `backend == nullptr`

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context) override;
```

**Parameters**:
- `context`: `SimulationContext&` – registry/timing handle for the current tick

**Returns**: `void`

**Exceptions**: Propagates backend failures

**Complexity**: O(N + P) where N = body count, P = active contact pairs

**Side Effects**: Calls backend `sync_from_registry()` and `step()`, updates cached statistics

---

### name()

```cpp
[[nodiscard]] std::string_view name() const override noexcept;
```

**Returns**: `std::string_view` – always `"physics"`

**Exceptions**: None

**Complexity**: O(1)

---

### stats()

```cpp
[[nodiscard]] const IPhysicsBackend::Stats& stats() const noexcept;
```

**Returns**: Cached backend statistics from the previous tick

**Exceptions**: None

**Complexity**: O(1)

---

## MetabolismSystem

### Constructor

```cpp
explicit MetabolismSystem(bool destroy_on_zero = true) noexcept;
```

**Parameters**:
- `destroy_on_zero`: `bool` – Whether entities are destroyed once `energy <= 0`

**Returns**: `MetabolismSystem`

**Exceptions**: None

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context) override;
```

**Parameters**:
- `context`: `SimulationContext&` – Registry/timing handle for current tick

**Returns**: `void`

**Exceptions**: None

**Complexity**: O(N) where N = entities with `MetabolismComponent`

**Side Effects**:
- Clamps `MetabolismComponent::energy`
- Optionally destroys entities depleted of energy

---

### name()

```cpp
[[nodiscard]] std::string_view name() const override noexcept;
```

**Returns**: `std::string_view` literal `"metabolism"`

**Exceptions**: None

**Complexity**: O(1)

---

### set_destroy_on_zero()

```cpp
void set_destroy_on_zero(bool enabled) noexcept;
```

**Parameters**:
- `enabled`: `bool` – Enables or disables destruction of depleted entities

**Returns**: `void`

**Exceptions**: None

**Complexity**: O(1)

**Side Effects**: Updates internal policy applied on subsequent ticks

---

## FitnessUpdateSystem

### Constructor

```cpp
explicit FitnessUpdateSystem(FitnessWeights weights = {}) noexcept;
```

**Parameters**:
- `weights`: `FitnessWeights` – Coefficients applied to age, energy integral, and offspring count  
  (`double age_weight`, `double energy_weight`, `double offspring_weight`)

**Returns**: `FitnessUpdateSystem`

**Exceptions**: None

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context) override;
```

**Parameters**:
- `context`: `SimulationContext&` – Provides registry access and timing metadata

**Returns**: `void`

**Exceptions**: None

**Complexity**: O(N) where N = entities containing both `FitnessComponent` and `MetabolismComponent`

**Side Effects**:
- Increments `FitnessComponent::age_seconds` and `energy_int_accum`
- Recomputes `FitnessComponent::last_fitness`

---

### name()

```cpp
[[nodiscard]] std::string_view name() const noexcept override;
```

**Returns**: `std::string_view` literal `"fitness_update"`

**Exceptions**: None

**Complexity**: O(1)

---

## StatsSystem

### Constructor

```cpp
explicit StatsSystem(double interval_seconds = 1.0) noexcept;
```

**Parameters**:
- `interval_seconds`: `double` – Seconds between log statements (`<= 0` → every tick)

**Returns**: `StatsSystem`

**Exceptions**: None

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context) override;
```

**Parameters**:
- `context`: `SimulationContext&` – Registry/timing handle for the current tick

**Returns**: `void`

**Exceptions**: None

**Complexity**: O(N) where N = entities with `MetabolismComponent`

**Side Effects**: Emits `spdlog::info` messages when the interval elapses

---

### name()

```cpp
[[nodiscard]] std::string_view name() const override noexcept;
```

**Returns**: `std::string_view` literal `"stats"`

**Exceptions**: None

**Complexity**: O(1)

---

### set_interval()

```cpp
void set_interval(double interval_seconds) noexcept;
```

**Parameters**:
- `interval_seconds`: `double` – Seconds between log statements (`<= 0` → every tick)

**Returns**: `void`

**Exceptions**: None

**Complexity**: O(1)

**Side Effects**: Updates scheduling cadence for subsequent ticks

---

## IPhysicsBackend

Interface declared in `sim/include/evolution/sim/physics/backend.h`.

### configure()

```cpp
virtual void configure(const Config& config) = 0;
```

**Parameters**:
- `config`: `const Config&` – cell size, solver iterations, Baumgarte factor, slop threshold

**Returns**: `void`

---

### sync_from_registry()

```cpp
virtual void sync_from_registry(entt::registry& registry) = 0;
```

**Parameters**:
- `registry`: `entt::registry&` – ECS storage providing physics components

**Returns**: `void`

**Complexity**: O(N) where N = registered bodies

---

### step()

```cpp
virtual void step(entt::registry& registry, double dt) = 0;
```

**Parameters**:
- `registry`: `entt::registry&` – ECS storage to mutate
- `dt`: `double` – timestep in seconds

**Returns**: `void`

**Complexity**: O(N + P)

---

### raycast()

```cpp
[[nodiscard]] virtual std::optional<ContactEvent>
raycast(const Vec3& origin, const Vec3& direction, double max_distance) const = 0;
```

**Parameters**:
- `origin`: `const Vec3&`
- `direction`: `const Vec3&` – normalized ray direction
- `max_distance`: `double` – maximum query distance

**Returns**: Closest hit if supported, otherwise `std::nullopt`

---

### contact_events()

```cpp
[[nodiscard]] virtual std::span<const ContactEvent> contact_events() const = 0;
```

**Returns**: Span of per-tick contact events (valid until next `step()`)

---

### stats()

```cpp
[[nodiscard]] virtual Stats stats() const = 0;
```

**Returns**: Aggregated diagnostics (body count, pair count, contact count, iterations)

---

## SimplePhysicsBackend

Declared in `sim/include/evolution/sim/physics/simple_backend.h`.

### Constructor

```cpp
explicit SimplePhysicsBackend(const SimplePhysicsConfig& config);
```

**Parameters**:
- `config`: `const SimplePhysicsConfig&` – gravity, ground height, and core solver settings

**Returns**: `SimplePhysicsBackend`

**Exceptions**: None

---

### set_gravity()

```cpp
void set_gravity(const Vec3& gravity) noexcept;
```

**Parameters**:
- `gravity`: `const Vec3&` – acceleration applied during force integration

**Returns**: `void`

---

### set_ground_height()

```cpp
void set_ground_height(double ground_y) noexcept;
```

**Parameters**:
- `ground_y`: `double` – world-space Y coordinate of the infinite plane

**Returns**: `void`

---

## Vec3

### Constructor (default)

```cpp
constexpr Vec3() = default;
```

**Parameters**: None

**Returns**: `Vec3` initialized to `{0.0, 0.0, 0.0}`

**Exceptions**: None

**Complexity**: O(1)

---

### Constructor (from components)

```cpp
constexpr Vec3(double x_, double y_, double z_);
```

**Parameters**:
- `x_`: `double` - X component
- `y_`: `double` - Y component
- `z_`: `double` - Z component

**Returns**: `Vec3` with specified components

**Exceptions**: None

**Complexity**: O(1)

---

### operator+=()

```cpp
Vec3& operator+=(const Vec3& rhs);
```

**Parameters**:
- `rhs`: `const Vec3&` - Right-hand operand

**Returns**: `Vec3&` - Reference to `*this` after modification

**Exceptions**: None

**Complexity**: O(1)

**Side Effects**: Adds `rhs` components to `*this`

---

### operator-=()

```cpp
Vec3& operator-=(const Vec3& rhs);
```

**Parameters**:
- `rhs`: `const Vec3&` - Right-hand operand

**Returns**: `Vec3&` - Reference to `*this` after modification

**Exceptions**: None

**Complexity**: O(1)

**Side Effects**: Subtracts `rhs` components from `*this`

---

### operator*=()

```cpp
Vec3& operator*=(double scalar);
```

**Parameters**:
- `scalar`: `double` - Scaling factor

**Returns**: `Vec3&` - Reference to `*this` after modification

**Exceptions**: None

**Complexity**: O(1)

**Side Effects**: Multiplies all components by `scalar`

---

### length_squared()

```cpp
[[nodiscard]] double length_squared() const;
```

**Parameters**: None

**Returns**: `double` - Squared Euclidean length (x² + y² + z²)

**Exceptions**: None

**Complexity**: O(1)

---

### length()

```cpp
[[nodiscard]] double length() const;
```

**Parameters**: None

**Returns**: `double` - Euclidean length (√(x² + y² + z²))

**Exceptions**: None

**Complexity**: O(1)

**Note**: Uses `std::sqrt()`, may be slower than `length_squared()`

---

### operator+() (free function)

```cpp
Vec3 operator+(Vec3 lhs, const Vec3& rhs);
```

**Parameters**:
- `lhs`: `Vec3` - Left-hand operand (copied)
- `rhs`: `const Vec3&` - Right-hand operand

**Returns**: `Vec3` - Component-wise sum

**Exceptions**: None

**Complexity**: O(1)

---

### operator-() (free function)

```cpp
Vec3 operator-(Vec3 lhs, const Vec3& rhs);
```

**Parameters**:
- `lhs`: `Vec3` - Left-hand operand (copied)
- `rhs`: `const Vec3&` - Right-hand operand

**Returns**: `Vec3` - Component-wise difference

**Exceptions**: None

**Complexity**: O(1)

---

### operator*() (vector × scalar)

```cpp
Vec3 operator*(Vec3 lhs, double scalar);
```

**Parameters**:
- `lhs`: `Vec3` - Vector operand (copied)
- `scalar`: `double` - Scaling factor

**Returns**: `Vec3` - Scaled vector

**Exceptions**: None

**Complexity**: O(1)

---

### operator*() (scalar × vector)

```cpp
Vec3 operator*(double scalar, Vec3 rhs);
```

**Parameters**:
- `scalar`: `double` - Scaling factor
- `rhs`: `Vec3` - Vector operand (copied)

**Returns**: `Vec3` - Scaled vector

**Exceptions**: None

**Complexity**: O(1)

---

## Component Access (EnTT Registry)

### get()

```cpp
template<typename Component>
Component& registry.get<Component>(entt::entity entity);
```

**Parameters**:
- `entity`: `entt::entity` - Entity identifier
  - Precondition: Entity must have `Component` attached

**Returns**: `Component&` - Reference to component

**Exceptions**: Throws if component not attached

**Complexity**: O(1)

---

### emplace()

```cpp
template<typename Component, typename... Args>
Component& registry.emplace<Component>(entt::entity entity, Args&&... args);
```

**Parameters**:
- `entity`: `entt::entity` - Entity identifier
- `args...`: `Args&&...` - Constructor arguments for component

**Returns**: `Component&` - Reference to newly created component

**Exceptions**: May throw `std::bad_alloc`

**Complexity**: O(1) amortized

**Side Effects**: Attaches component to entity (creates if not exists)

---

### view()

```cpp
template<typename... Component>
auto registry.view<Component...>();
```

**Parameters**: None (template parameters specify component types)

**Returns**: View object for iterating entities with specified components

**Exceptions**: None

**Complexity**: O(1) to create view, O(N) to iterate N entities

**Usage**:
```cpp
auto view = registry.view<TransformComponent, KinematicsComponent>();
view.each([](TransformComponent& transform, KinematicsComponent& kinematics) {
    // Process entities
});
```

---

## Terrain

### Constructor

```cpp
explicit Terrain(const TerrainConfig& config);
```

**Parameters**:
- `config`: `TerrainConfig` - Configuration for heightfield generation
  - `width_cells`: `int` - Grid width (default: 512)
  - `height_cells`: `int` - Grid height (default: 512)
  - `cell_size`: `double` - World-space cell size in meters (default: 1.0)
  - `elevation_scale`: `double` - Maximum height amplitude (default: 20.0)
  - `seed`: `unsigned int` - Noise seed (default: 1337)
  - `octaves`: `int` - Fractal noise octaves (default: 5)
  - `base_frequency`: `double` - Base frequency (default: 0.005)
  - `lacunarity`: `double` - Frequency multiplier (default: 2.0)
  - `gain`: `double` - Amplitude multiplier (default: 0.5)

**Returns**: `Terrain` instance

**Exceptions**: May throw `std::bad_alloc` if memory allocation fails

**Complexity**: O(W×H) where W×H = grid resolution

**Side Effects**: Generates heightfield samples from noise

---

### height()

```cpp
double height(double x, double z) const noexcept;
```

**Parameters**:
- `x`: `double` - World-space X coordinate in meters
- `z`: `double` - World-space Z coordinate in meters

**Returns**: `double` - Terrain elevation in meters

**Exceptions**: None (noexcept)

**Complexity**: O(1) - bilinear interpolation

**Warning**: Heights queried outside domain are clamped to edge values

---

### normal()

```cpp
Vec3 normal(double x, double z) const noexcept;
```

**Parameters**:
- `x`: `double` - World-space X coordinate
- `z`: `double` - World-space Z coordinate

**Returns**: `Vec3` - Unit-length surface normal vector

**Exceptions**: None (noexcept)

**Complexity**: O(1) - central differences

---

## SoilGrid

### Constructor

```cpp
explicit SoilGrid(const SoilConfig& config);
```

**Parameters**:
- `config`: `SoilConfig` - Grid configuration
  - `width_cells`: `int` - Grid width (default: 256)
  - `height_cells`: `int` - Grid height (default: 256)
  - `cell_size`: `double` - World-space cell size (default: 2.0)
  - `max_nutrient`: `float` - Upper bound per cell (default: 10.0)
  - `diffusion_rate`: `float` - Diffusion coefficient (default: 0.5)
  - `regeneration_rate`: `float` - Baseline regen per second (default: 0.05)
  - `baseline_nutrient`: `float` - Floor value (default: 4.0)

**Returns**: `SoilGrid` instance

**Exceptions**: May throw `std::bad_alloc`

**Complexity**: O(W×H) for grid allocation

---

### sample()

```cpp
float sample(double x, double z) const noexcept;
```

**Parameters**:
- `x`: `double` - World-space X coordinate in meters
- `z`: `double` - World-space Z coordinate in meters

**Returns**: `float` - Bilinearly interpolated nutrient value

**Exceptions**: None (noexcept)

**Complexity**: O(1)

---

### diffuse()

```cpp
void diffuse(double dt) noexcept;
```

**Parameters**:
- `dt`: `double` - Simulation timestep in seconds

**Returns**: `void`

**Exceptions**: None (noexcept)

**Complexity**: O(W×H) - five-point stencil

**Note**: Uses internal scratch buffer; conserves mass except for clamping

---

### regenerate()

```cpp
void regenerate(double dt) noexcept;
```

**Parameters**:
- `dt`: `double` - Simulation timestep in seconds

**Returns**: `void`

**Exceptions**: None (noexcept)

**Complexity**: O(W×H)

**Note**: Regenerates nutrients toward baseline value

---

## EnvironmentBootstrapSystem

### Constructor

```cpp
explicit EnvironmentBootstrapSystem(const TerrainConfig& terrain_config, const SoilConfig& soil_config);
```

**Parameters**:
- `terrain_config`: `TerrainConfig` - Terrain generation parameters
- `soil_config`: `SoilConfig` - Soil grid parameters

**Returns**: `EnvironmentBootstrapSystem` instance

**Exceptions**: May propagate exceptions from terrain/soil construction

**Complexity**: O(W×H) for terrain + soil initialization

**Side Effects**: Creates terrain and soil grid, stores in `registry.ctx()`

**Note**: Runs once on first tick, then disables itself

---

## FeedingSystem

### Constructor

```cpp
FeedingSystem() = default;
```

**Parameters**: None

**Returns**: `FeedingSystem` instance

**Exceptions**: None

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context) override;
```

**Parameters**:
- `context`: `SimulationContext&` - Tick-scoped simulation state

**Returns**: `void`

**Exceptions**: May propagate exceptions from registry access

**Complexity**: O(H × P_avg) where H = herbivore count, P_avg = average plants in radius

**Side Effects**:
- Transfers energy from plants to herbivores
- Updates `MetabolismComponent::energy` and `PlantComponent::energy`
- Marks plants dead if energy depleted

**Component Requirements**:
- Herbivore: `TransformComponent`, `MetabolismComponent`, `FeedingIntent`, `HerbivoreTag`
- Plant: `TransformComponent`, `PlantComponent`

---

## PlantGrowthSystem

### Constructor

```cpp
PlantGrowthSystem() = default;
```

**Parameters**: None

**Returns**: `PlantGrowthSystem` instance

**Exceptions**: None

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context) override;
```

**Parameters**:
- `context`: `SimulationContext&` - Tick-scoped simulation state

**Returns**: `void`

**Exceptions**: May propagate exceptions from registry access

**Complexity**: O(N) where N = plant count

**Side Effects**: Increases plant energy based on soil nutrients

**Component Requirements**: `TransformComponent`, `PlantComponent`

---

## PlantSeedingSystem

### Constructor

```cpp
PlantSeedingSystem() = default;
```

**Parameters**: None

**Returns**: `PlantSeedingSystem` instance

**Exceptions**: None

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context) override;
```

**Parameters**:
- `context`: `SimulationContext&` - Tick-scoped simulation state

**Returns**: `void`

**Exceptions**: May propagate exceptions from registry access

**Complexity**: O(N) with occasional spawns

**Side Effects**: Spawns new plants when parent conditions met

**Component Requirements**: `TransformComponent`, `PlantComponent`, `PlantSeedParams`

---

## PlantCleanupSystem

### Constructor

```cpp
PlantCleanupSystem() = default;
```

**Parameters**: None

**Returns**: `PlantCleanupSystem` instance

**Exceptions**: None

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context) override;
```

**Parameters**:
- `context`: `SimulationContext&` - Tick-scoped simulation state

**Returns**: `void`

**Exceptions**: May propagate exceptions from registry access

**Complexity**: O(N) where N = dead plant count

**Side Effects**: Destroys dead plant entities after delay

**Component Requirements**: `PlantComponent`

---

## SoilSystem

### Constructor

```cpp
explicit SoilSystem(double diffusion_interval = 0.1) noexcept;
```

**Parameters**:
- `diffusion_interval`: `double` - Time between diffusion updates (default: 0.1s)

**Returns**: `SoilSystem` instance

**Exceptions**: None (noexcept)

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context) override;
```

**Parameters**:
- `context`: `SimulationContext&` - Tick-scoped simulation state

**Returns**: `void`

**Exceptions**: May propagate exceptions from registry access

**Complexity**: O(W×H) when diffusion runs (sub-sampled)

**Side Effects**: Updates soil nutrient diffusion and regeneration

**Service Requirements**: `SoilGrid` in `registry.ctx()`
---

## TelemetrySystem

### Construction

```cpp
explicit TelemetrySystem(const std::filesystem::path& output_dir,
                         std::string_view run_id = "default",
                         const TelemetryTargeting& targeting = {},
                         const RollupConfig& rollup_config = {});
```

**Parameters**:
- `output_dir`: `std::filesystem::path` - Output root for telemetry files
- `run_id`: `std::string_view` - Identifier for the run (used in output records)
- `targeting`: `TelemetryTargeting` - Filtering and sampling configuration
- `rollup_config`: `RollupConfig` - Rollup cadence and buffer configuration

**Returns**: `TelemetrySystem` instance

**Exceptions**: `std::filesystem::filesystem_error` if output directory cannot be created

**Complexity**: O(1)

---

### tick()

```cpp
void tick(SimulationContext& context);
```

**Parameters**:
- `context`: `SimulationContext&` - Registry access and timing metadata

**Returns**: `void`

**Exceptions**: None (errors are logged)

**Complexity**: O(S + E) where S = species count, E = entity count

**Side Effects**:
- Emits rollup snapshots on cadence
- Emits movement metrics
- Flushes buffered events when buffer size threshold is reached

---

### emit_event()

```cpp
bool emit_event(const TelemetryEvent& event, bool force_capture = false);
```

**Parameters**:
- `event`: `TelemetryEvent` - Event record to buffer
- `force_capture`: `bool` - If true, bypass sampling/targeting filters

**Returns**: `bool` - True when event is captured

**Exceptions**: None

**Complexity**: O(1) amortized

---

### flush()

```cpp
void flush();
```

**Parameters**: None

**Returns**: `void`

**Exceptions**: `std::filesystem::filesystem_error` on write failure

**Complexity**: O(N) where N = buffered events

---

### should_capture()

```cpp
[[nodiscard]] bool should_capture(entt::entity entity,
                                  SpeciesId species_id = 0,
                                  genetics::GenomeId genome_id = 0) const noexcept;
```

**Parameters**:
- `entity`: `entt::entity` - Entity identifier
- `species_id`: `SpeciesId` - Species identifier (optional)
- `genome_id`: `genetics::GenomeId` - Genome identifier (optional)

**Returns**: `bool` - True when entity matches targeting rules

**Exceptions**: None

**Complexity**: O(1)

---

### should_sample()

```cpp
[[nodiscard]] bool should_sample() const noexcept;
```

**Parameters**: None

**Returns**: `bool` - True when non-targeted sampling should occur

**Exceptions**: None

**Complexity**: O(1)

---

### set_targeting()

```cpp
void set_targeting(const TelemetryTargeting& targeting) noexcept;
```

**Parameters**:
- `targeting`: `TelemetryTargeting` - New targeting configuration

**Returns**: `void`

**Exceptions**: None

**Complexity**: O(1)
