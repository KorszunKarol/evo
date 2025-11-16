# Module: Components (ECS)

## Overview

The components module defines all Entity Component System (ECS) component types used throughout the simulation. Components are plain data structures that store entity state.

## Files

- `sim/include/evolution/sim/components.h`

## Component Types

### TransformComponent

**Purpose**: Stores spatial information for an entity within the simulation world.

**Structure**:
```cpp
struct TransformComponent {
    Vec3 position{0.0, 0.0, 0.0};
};
```

**Fields**:

- `position`: `Vec3`
  - **Type**: 3D vector
  - **Units**: Meters (world space)
  - **Default**: `{0.0, 0.0, 0.0}` (origin)
  - **Range**: Any finite value
  - **Precision**: Double precision floating point
  - **Usage**: World-space position of entity center

**Data Contract**:

- **Read by**: Physics systems, rendering systems, spatial queries
- **Written by**: Physics systems, movement systems, spawn systems
- **Thread safety**: Not thread-safe (requires external synchronization)
- **Lifetime**: Tied to entity lifetime (destroyed when entity destroyed)

**Usage Pattern**:
```cpp
// Read position
auto& transform = registry.get<TransformComponent>(entity);
Vec3 pos = transform.position;

// Modify position
transform.position = {10.0, 5.0, 0.0};
```

**Future Extensions**:
- `rotation`: Quaternion or Euler angles (when rotation needed)
- `scale`: Uniform or non-uniform scaling (when morphology evolves)

### KinematicsComponent

**Purpose**: Contains velocity and force accumulation for simple integration.

**Structure**:
```cpp
struct KinematicsComponent {
    Vec3 linear_velocity{0.0, 0.0, 0.0};
    Vec3 accumulated_force{0.0, 0.0, 0.0};
    double inverse_mass{1.0};
    double linear_damping{0.05};
    double restitution{0.0};
    double friction{0.6};
};
```

**Fields**:

- `linear_velocity`: `Vec3`
  - **Type**: 3D velocity vector
  - **Units**: Meters per second
  - **Default**: `{0.0, 0.0, 0.0}` (stationary)
  - **Range**: Any finite value
  - **Usage**: Current linear velocity of entity
  - **Modified by**: Physics integration systems

- `accumulated_force`: `Vec3`
  - **Type**: 3D force vector
  - **Units**: Newtons (force units)
  - **Default**: `{0.0, 0.0, 0.0}` (no forces)
  - **Range**: Any finite value
  - **Usage**: Forces accumulated during tick (reset after integration)
  - **Modified by**: Systems that apply forces (muscles, collisions, etc.)
  - **Reset by**: Physics system after integration

- `inverse_mass`: `double`
  - **Type**: Reciprocal of mass
  - **Units**: 1/kg
  - **Default**: `1.0` (unit mass)
  - **Range**: `[0, ∞)`
  - **Special values**:
    - `0.0`: Infinite mass (static/immovable body)
    - `> 0.0`: Dynamic body (mass = 1.0 / inverse_mass)
  - **Usage**: Simplifies physics calculations (F = ma → a = F * inverse_mass)
  - **Why inverse?**: Avoids division in hot path, zero naturally represents static

- `linear_damping`: `double`
  - **Type**: Dimensionless damping coefficient
  - **Units**: Multiplier on velocity decay per second
  - **Default**: `0.05`
  - **Range**: `[0.0, 1.0]` typically; higher values clamp to zero velocity faster
  - **Usage**: Applies exponential decay to velocity each tick, approximating drag

- `restitution`: `double`
  - **Type**: Coefficient of restitution
  - **Units**: Dimensionless ratio for energy retention in collisions
  - **Default**: `0.0` (perfectly inelastic)
  - **Range**: `[0.0, 1.0]`
  - **Usage**: Controls bounce intensity when the entity collides with surfaces

- `friction`: `double`
  - **Type**: Coefficient of friction
  - **Units**: Dimensionless scalar used for tangential damping on contact
  - **Default**: `0.6`
  - **Range**: `[0.0, ∞)` though `[0.0, 1.0]` is recommended
  - **Usage**: Reduces horizontal velocity when the entity is in surface contact

**Data Contract**:

- **Read by**: Physics systems, collision systems
- **Written by**: Physics systems (velocity, force reset), force-applying systems
- **Thread safety**: Not thread-safe
- **Lifetime**: Tied to entity lifetime

**Usage Pattern**:
```cpp
// Apply force (before physics tick)
auto& kinematics = registry.get<KinematicsComponent>(entity);
kinematics.accumulated_force += Vec3{10.0, 0.0, 0.0};  // Push right

// Physics system reads and integrates:
// acceleration = accumulated_force * inverse_mass
// velocity += acceleration * dt
// position += velocity * dt
// accumulated_force = {0, 0, 0}  // Reset
```

**Static Body Pattern**:
```cpp
// Make entity static (immovable)
kinematics.inverse_mass = 0.0;
// Physics system will skip integration for this entity
```

### PhysicsMaterial

**Purpose**: Describes per-collider material properties used during collision response.

**Structure**:
```cpp
struct PhysicsMaterial {
    double friction{0.6};
    double restitution{0.0};
};
```

**Fields**:

- `friction`: `double`
  - **Type**: Coefficient of friction
  - **Units**: Dimensionless
  - **Default**: `0.6`
  - **Range**: `[0.0, 1.0]` recommended
  - **Usage**: Combined via geometric mean to clamp tangential impulses during contact resolution

- `restitution`: `double`
  - **Type**: Coefficient of restitution
  - **Units**: Dimensionless
  - **Default**: `0.0`
  - **Range**: `[0.0, 1.0]`
  - **Usage**: Maximum value across colliding surfaces, controls bounce amount

### CollisionFilter

**Purpose**: Controls which colliders interact and whether they generate impulses or only trigger events.

**Structure**:
```cpp
struct CollisionFilter {
    std::uint32_t category{0x1};
    std::uint32_t mask{0xFFFFFFFF};
    bool is_trigger{false};
};
```

**Fields**:

- `category`: `std::uint32_t`
  - **Type**: Bitfield representing the collider's own layer
  - **Default**: `0x1`
  - **Usage**: Assigned per gameplay/system requirement

- `mask`: `std::uint32_t`
  - **Type**: Bitmask of categories allowed to interact
  - **Default**: `0xFFFFFFFF`
  - **Usage**: `(categoryA & maskB) != 0` and `(categoryB & maskA) != 0` must hold for contacts

- `is_trigger`: `bool`
  - **Default**: `false`
  - **Usage**: When true, contacts are reported but solver skips impulse application

### ColliderComponent

**Purpose**: Encapsulates shape, material, and filter data required for collision detection.

**Structure**:
```cpp
enum class ShapeType { Sphere, Aabb, CapsuleY };

struct SphereCollider { double radius{0.5}; };
struct AabbCollider { Vec3 half_extents{0.5, 0.5, 0.5}; };
struct CapsuleYCollider { double radius{0.3}; double half_height{0.7}; };

struct ColliderComponent {
    ShapeType type{ShapeType::Sphere};
    Vec3 offset{0.0, 0.0, 0.0};
    PhysicsMaterial material{};
    CollisionFilter filter{};
    SphereCollider sphere{};
    AabbCollider aabb{};
    CapsuleYCollider capsule{};
};
```

**Fields**:

- `type`: `ShapeType`
  - **Usage**: Indicates which payload is active (sphere/AABB/upright capsule)

- `offset`: `Vec3`
  - **Usage**: Local offset from the entity transform, supports asymmetrical shapes

- `material`: `PhysicsMaterial`
  - **Usage**: Overrides default damping/restitution/fraction during collision response

- `filter`: `CollisionFilter`
  - **Usage**: Controls collision layers and trigger behavior

- `sphere`: `SphereCollider`
  - **Usage**: Valid when `type == ShapeType::Sphere`, holds radius

- `aabb`: `AabbCollider`
  - **Usage**: Valid when `type == ShapeType::Aabb`, stores half extents

- `capsule`: `CapsuleYCollider`
  - **Usage**: Valid when `type == ShapeType::CapsuleY`, stores radius and half height

### RigidbodyComponent

**Purpose**: Flag component influencing how a body participates in physics resolution.

**Structure**:
```cpp
struct RigidbodyComponent {
    bool is_static{false};
    bool is_kinematic{false};
};
```

**Fields**:

- `is_static`: `bool`
  - **Default**: `false`
  - **Usage**: Forces the physics backend to treat the body as immovable even if inverse mass is non-zero

- `is_kinematic`: `bool`
  - **Default**: `false`
  - **Usage**: Bodies moved by gameplay/scripting; collisions emit events without impulses

### MetabolismComponent

**Purpose**: Tracks the energetic state of an entity.

**Structure**:
```cpp
struct MetabolismComponent {
    double energy{100.0};
    double max_energy{100.0};
    double basal_rate{1.0};
};
```

**Fields**:

- `energy`: `double`
  - **Type**: Current energy reserve
  - **Units**: Arbitrary simulation units (not Joules)
  - **Default**: `100.0`
  - **Range**: `[0.0, max_energy]`
  - **Usage**: Current available energy for actions
  - **Modified by**: Metabolism systems, action systems, feeding systems
  - **Constraints**: Should be clamped to `[0, max_energy]` (caller responsibility)

- `max_energy`: `double`
  - **Type**: Maximum energy capacity
  - **Units**: Arbitrary simulation units
  - **Default**: `100.0`
  - **Range**: `(0.0, ∞)`
  - **Usage**: Upper bound for energy storage
  - **Modified by**: Evolution/genetics (future), growth systems (future)
  - **Immutable**: Typically set at entity creation

- `basal_rate`: `double`
  - **Type**: Basal metabolic consumption rate
  - **Units**: Energy per second
  - **Default**: `1.0`
  - **Range**: `[0.0, ∞)`
  - **Usage**: Passive energy consumption per second
  - **Modified by**: Evolution/genetics (future)
  - **Applied by**: Metabolism system each tick

**Data Contract**:

- **Read by**: Metabolism systems, action systems (check if enough energy)
- **Written by**: Metabolism systems (consume energy), feeding systems (add energy)
- **Thread safety**: Not thread-safe
- **Lifetime**: Tied to entity lifetime

**Usage Pattern**:
```cpp
// Check if entity has enough energy for action
auto& metabolism = registry.get<MetabolismComponent>(entity);
if (metabolism.energy >= action_cost) {
    metabolism.energy -= action_cost;
    // Perform action
}

// Metabolism system consumes basal rate each tick:
// metabolism.energy -= metabolism.basal_rate * dt;
// if (metabolism.energy < 0.0) metabolism.energy = 0.0;
```

**Energy Death Pattern**:
```cpp
// Entity dies when energy reaches zero
if (metabolism.energy <= 0.0) {
    registry.destroy(entity);
}
```

### FitnessComponent

**Purpose**: Accumulates deterministic fitness metrics for selection and reproduction systems.

**Structure**:
```cpp
struct FitnessComponent {
    double age_seconds{0.0};
    double energy_int_accum{0.0};
    std::uint32_t offspring_count{0};
    double last_fitness{0.0};
};
```

**Fields**:

- `age_seconds`: `double`
  - **Type**: Simulation age
  - **Units**: Seconds
  - **Default**: `0.0`
  - **Usage**: Incremented once per tick by `FitnessUpdateSystem`

- `energy_int_accum`: `double`
  - **Type**: Time-integrated energy
  - **Units**: Energy-seconds
  - **Default**: `0.0`
  - **Usage**: Accumulates `MetabolismComponent::energy * dt`

- `offspring_count`: `std::uint32_t`
  - **Type**: Spawned offspring counter
  - **Default**: `0`
  - **Usage**: Incremented by reproduction systems when new entities are created

- `last_fitness`: `double`
  - **Type**: Scalar fitness score
  - **Default**: `0.0`
  - **Usage**: Computed each tick from age, energy integral, and offspring count

**Data Contract**:

- **Read by**: Species indexing, reproduction, selection, telemetry
- **Written by**: `FitnessUpdateSystem` (age/energy/score), reproduction systems (offspring count)
- **Thread safety**: Not thread-safe

**Usage Pattern**:
```cpp
auto entity = registry.create();
registry.emplace<FitnessComponent>(entity, FitnessComponent{});
// FitnessUpdateSystem will populate age/energy/final score each tick.
```

### GenomeHandleComponent

**Purpose**: Associates an entity with a genome stored in external storage.

**Structure**:
```cpp
struct GenomeHandleComponent {
    std::uint64_t id{0};
};
```

**Fields**:

- `id`: `std::uint64_t`
  - **Type**: 64-bit unsigned integer
  - **Default**: `0` (invalid/uninitialized)
  - **Range**: `[1, 2^64-1]` for valid IDs, `0` for invalid
  - **Usage**: Unique identifier for genome lookup
  - **Modified by**: Spawn systems (set at creation), reproduction systems (inherit/modify)
  - **Lookup**: Used to retrieve genome data from genome storage (future)

**Data Contract**:

- **Read by**: Genome systems, reproduction systems, evolution systems
- **Written by**: Spawn systems, reproduction systems
- **Thread safety**: Not thread-safe
- **Lifetime**: Tied to entity lifetime

**Usage Pattern**:
```cpp
// Set genome ID at entity creation
auto& genome = registry.emplace<GenomeHandleComponent>(entity);
genome.id = genome_storage.allocate_new_genome();

// Lookup genome data (future):
// const GenomeData& data = genome_storage.get(genome.id);
```

**Invalid ID Handling**:
- `id == 0`: Invalid/uninitialized (entity has no genome)
- Valid IDs: `>= 1` (implementation-defined range)

### NameComponent

**Purpose**: Optional human-readable name for debug visualization.

**Structure**:
```cpp
struct NameComponent {
    std::string value;
};
```

**Fields**:

- `value`: `std::string`
  - **Type**: UTF-8 encoded string
  - **Default**: Empty string `""`
  - **Range**: Any valid UTF-8 string
  - **Usage**: Debug labels, logging, visualization
  - **Modified by**: Spawn systems, debug tools
  - **Optional**: Entity can exist without this component

**Data Contract**:

- **Read by**: Debug systems, logging systems, visualization systems
- **Written by**: Spawn systems, debug tools
- **Thread safety**: Not thread-safe
- **Lifetime**: Tied to entity lifetime

**Usage Pattern**:
```cpp
// Add name for debugging
registry.emplace<NameComponent>(entity, NameComponent{.value = "creature_123"});

// Use in logging
if (auto* name = registry.try_get<NameComponent>(entity)) {
    spdlog::info("Entity {} at position {}", name->value, transform.position);
}
```

### PlantComponent

**Purpose**: Tracks the energetic state and lifecycle of a plant entity.

**Structure**:
```cpp
struct PlantComponent {
    double energy;              // Current edible store
    double max_energy;         // Maximum capacity
    double growth_rate;        // Energy/sec from soil + light
    double radius;             // Edible/contact radius (m)
    double seed_interval;      // Seconds between seed attempts
    double seed_timer;         // Accumulates toward seeding
    double cleanup_delay;      // Delay before culling (s)
    bool alive;                // Fast filter
};
```

**Fields**:

- `energy`: `double`
  - **Type**: Current edible energy store
  - **Units**: Arbitrary simulation units
  - **Default**: `10.0`
  - **Range**: `[0.0, max_energy]`
  - **Usage**: Consumed by herbivores, increases from growth
  - **Modified by**: PlantGrowthSystem, FeedingSystem, PlantSeedingSystem

- `max_energy`: `double`
  - **Type**: Maximum energy capacity
  - **Units**: Arbitrary simulation units
  - **Default**: `20.0`
  - **Range**: `(0.0, ∞)`
  - **Usage**: Upper bound for energy storage
  - **Immutable**: Set at plant creation

- `growth_rate`: `double`
  - **Type**: Energy generation rate
  - **Units**: Energy per second
  - **Default**: `2.0`
  - **Range**: `[0.0, ∞)`
  - **Usage**: Applied by PlantGrowthSystem based on soil availability

- `radius`: `double`
  - **Type**: Edible/contact radius
  - **Units**: Meters
  - **Default**: `0.5`
  - **Range**: `(0.0, ∞)`
  - **Usage**: Determines feeding reach distance

- `seed_interval`: `double`
  - **Type**: Time between seed attempts
  - **Units**: Seconds
  - **Default**: `30.0`
  - **Range**: `(0.0, ∞)`
  - **Usage**: Controls reproduction frequency

- `seed_timer`: `double`
  - **Type**: Accumulated time since last seed
  - **Units**: Seconds
  - **Default**: `0.0`
  - **Range**: `[0.0, ∞)`
  - **Usage**: Incremented each tick, reset after seeding

- `cleanup_delay`: `double`
  - **Type**: Delay before culling dead plants
  - **Units**: Seconds
  - **Default**: `10.0`
  - **Range**: `[0.0, ∞)`
  - **Usage**: Allows decomposition effects before removal

- `alive`: `bool`
  - **Type**: Life status flag
  - **Default**: `true`
  - **Usage**: Fast filter for active plants, set to false when depleted

**Data Contract**:

- **Read by**: FeedingSystem, PlantGrowthSystem, PlantSeedingSystem, PlantCleanupSystem
- **Written by**: PlantGrowthSystem (energy), PlantSeedingSystem (energy, timer), FeedingSystem (energy, alive)
- **Thread safety**: Not thread-safe
- **Lifetime**: Tied to entity lifetime

**Usage Pattern**:
```cpp
// Create plant
auto plant = registry.create();
registry.emplace<TransformComponent>(plant, TransformComponent{.position = {x, y, z}});
registry.emplace<PlantComponent>(plant, PlantComponent{
    .energy = 10.0,
    .max_energy = 20.0,
    .growth_rate = 2.0,
    .radius = 0.5
});
```

### PlantSeedParams

**Purpose**: Parameters controlling plant seed spawning behavior.

**Structure**:
```cpp
struct PlantSeedParams {
    double seed_min_energy;      // Parent threshold to seed
    double seed_cost;             // Energy removed from parent
    double seed_radius;           // Spawn radius (m)
    double establish_probability; // Chance to take root
};
```

**Fields**:

- `seed_min_energy`: `double`
  - **Type**: Minimum energy required to seed
  - **Units**: Energy units
  - **Default**: `12.0`
  - **Usage**: Prevents seeding when parent is weak

- `seed_cost`: `double`
  - **Type**: Energy deducted from parent
  - **Units**: Energy units
  - **Default**: `4.0`
  - **Usage**: Reproduction cost

- `seed_radius`: `double`
  - **Type**: Maximum spawn distance from parent
  - **Units**: Meters
  - **Default**: `6.0`
  - **Usage**: Controls seed dispersal range

- `establish_probability`: `double`
  - **Type**: Success chance for seed establishment
  - **Range**: `[0.0, 1.0]`
  - **Default**: `0.65`
  - **Usage**: Accounts for terrain/soil suitability

**Data Contract**:

- **Read by**: PlantSeedingSystem
- **Written by**: Spawn systems (initialization only)
- **Thread safety**: Immutable after creation

### FeedingIntent

**Purpose**: Declares that an entity attempts to feed from nearby plants.

**Structure**:
```cpp
struct FeedingIntent {
    bool request_eat{true};  // For v1, always true when in range
    double reach{1.5};        // Max eat distance (m)
    double rate{6.0};         // Energy/sec transfer cap
};
```

**Fields**:

- `request_eat`: `bool`
  - **Type**: Feeding request flag
  - **Default**: `true`
  - **Usage**: For v1, always treated as true; future brain control

- `reach`: `double`
  - **Type**: Maximum feeding distance
  - **Units**: Meters
  - **Default**: `1.5`
  - **Range**: `(0.0, ∞)`
  - **Usage**: Combined with plant radius for collision detection

- `rate`: `double`
  - **Type**: Maximum energy transfer rate
  - **Units**: Energy per second
  - **Default**: `6.0`
  - **Range**: `[0.0, ∞)`
  - **Usage**: Caps feeding speed

**Data Contract**:

- **Read by**: FeedingSystem
- **Written by**: Brain systems (future), spawn systems (initialization)
- **Thread safety**: Not thread-safe

**Usage Pattern**:
```cpp
// Mark entity as herbivore
registry.emplace<HerbivoreTag>(entity);
registry.emplace<FeedingIntent>(entity, FeedingIntent{
    .reach = 2.0,
    .rate = 8.0
});
```

### HerbivoreTag

**Purpose**: Empty tag component marking entities that can consume plants.

**Structure**:
```cpp
struct HerbivoreTag {};
```

**Data Contract**:

- **Read by**: FeedingSystem (for filtering herbivores)
- **Written by**: Spawn systems
- **Thread safety**: Not applicable (tag only)

**Usage**: Used with `registry.view<HerbivoreTag, FeedingIntent>()` to find feeding entities.

### ActuationComponent

**Purpose**: Pending actuation commands produced by neural controllers.

**Structure**:
```cpp
struct ActuationComponent {
    double impulse_x{0.0};   // Planar impulse along X (N·s)
    double impulse_z{0.0};   // Planar impulse along Z (N·s)
    bool jump{false};        // Jump impulse flag
    bool eat{false};         // Feeding request
    int update_skip{0};      // Brain-controlled throttle
};
```

**Fields**:

- `impulse_x`, `impulse_z`: `double`
  - **Type**: Desired planar impulses
  - **Units**: Newton seconds
  - **Default**: `0.0`
  - **Usage**: Applied by MotorSystem to `KinematicsComponent::accumulated_force`

- `jump`: `bool`
  - **Type**: Jump impulse request
  - **Default**: `false`
  - **Usage**: Triggers upward impulse when true

- `eat`: `bool`
  - **Type**: Feeding behavior request
  - **Default**: `false`
  - **Usage**: Sets `FeedingIntent::request_eat` when true

- `update_skip`: `int`
  - **Type**: Brain-controlled update throttle
  - **Default**: `0`
  - **Usage**: Allows brain to skip inference updates

**Data Contract**:

- **Read by**: MotorSystem
- **Written by**: BrainInferenceSystem
- **Reset by**: MotorSystem (after applying impulses)
- **Thread safety**: Not thread-safe

### BrainComponent

**Purpose**: Runtime metadata for the brain controller attached to an entity.

**Structure**:
```cpp
struct BrainComponent {
    enum class Kind : std::uint8_t {
        MLP,
        NEAT,
    };
    Kind kind{Kind::MLP};
    std::uint32_t input_count{0};
    std::uint32_t output_count{0};
    double update_interval{0.1};
    double accumulator{0.0};
    std::uint32_t storage_index{std::numeric_limits<std::uint32_t>::max()};
};
```

**Fields**:

- `kind`: `Kind`
  - **Type**: Backend type enumerator
  - **Default**: `MLP`
  - **Usage**: Selects inference backend

- `input_count`, `output_count`: `std::uint32_t`
  - **Type**: Sensor/actuator counts
  - **Default**: `0`
  - **Usage**: Validates brain configuration

- `update_interval`: `double`
  - **Type**: Target time between inference updates
  - **Units**: Seconds
  - **Default**: `0.1`
  - **Usage**: Controls inference frequency

- `accumulator`: `double`
  - **Type**: Accumulated time since last inference
  - **Units**: Seconds
  - **Default**: `0.0`
  - **Usage**: Tracks when to run next inference

- `storage_index`: `std::uint32_t`
  - **Type**: Index into backend storage buffers
  - **Default**: `max()`
  - **Usage**: Future batching support

**Data Contract**:

- **Read by**: BrainInferenceSystem
- **Written by**: Spawn systems, BrainInferenceSystem (accumulator)
- **Thread safety**: Not thread-safe

### ReproductionComponent

**Purpose**: Reproduction policy and cooldown state for an entity.

**Structure**:
```cpp
struct ReproductionComponent {
    double cooldown{5.0};           // Min time between attempts (s)
    double timer{0.0};              // Time remaining (s)
    double mate_radius{3.0};        // Mate search radius (m)
    double energy_threshold{120.0};  // Min energy to reproduce
};
```

**Fields**:

- `cooldown`: `double`
  - **Type**: Minimum time between reproduction attempts
  - **Units**: Seconds
  - **Default**: `5.0`
  - **Usage**: Prevents rapid reproduction

- `timer`: `double`
  - **Type**: Time remaining before reproduction allowed
  - **Units**: Seconds
  - **Default**: `0.0`
  - **Range**: `[0.0, cooldown]`
  - **Usage**: Decremented each tick, reset after reproduction

- `mate_radius`: `double`
  - **Type**: Search radius for potential mates
  - **Units**: Meters
  - **Default**: `3.0`
  - **Usage**: Spatial query distance

- `energy_threshold`: `double`
  - **Type**: Minimum energy required to reproduce
  - **Units**: Energy units
  - **Default**: `120.0`
  - **Usage**: Ensures sufficient resources

**Data Contract**:

- **Read by**: ReproductionSystem (future)
- **Written by**: Spawn systems, ReproductionSystem (timer)
- **Thread safety**: Not thread-safe

## Component Relationships

### Required Combinations

- **Physics Entity**: `TransformComponent` + `KinematicsComponent`
- **Living Entity**: `MetabolismComponent` + `GenomeHandleComponent`
- **Evolving Creature**: `MetabolismComponent` + `GenomeHandleComponent` + `FitnessComponent`
- **Plant Entity**: `TransformComponent` + `PlantComponent`
- **Herbivore Entity**: `TransformComponent` + `MetabolismComponent` + `FeedingIntent` + `HerbivoreTag`
- **Brain Entity**: `BrainComponent` + `ActuationComponent` (future: + sensor components)
- **Named Entity**: Any component + `NameComponent` (optional)

### Component Dependencies

- **No hard dependencies**: Components are independent
- **Logical dependencies**: Systems may require multiple components
- **Optional components**: Entities can have subset of components

## Data Contracts Between Components

### TransformComponent ↔ KinematicsComponent

**Contract**: Physics systems read position, modify via velocity integration.

**Data Flow**:
- Physics reads: `TransformComponent::position`
- Physics reads: `KinematicsComponent::linear_velocity`
- Physics writes: `TransformComponent::position` (updated)
- Physics writes: `KinematicsComponent::linear_velocity` (updated)

**Synchronization**: Both modified atomically during physics tick

### KinematicsComponent ↔ MetabolismComponent

**Contract**: Movement consumes energy, energy affects movement capability.

**Data Flow**:
- Movement system reads: `MetabolismComponent::energy`
- Movement system writes: `MetabolismComponent::energy` (consumes)
- Movement system writes: `KinematicsComponent::accumulated_force` (applies force)

**Future**: Low energy → reduced movement speed

### PlantComponent ↔ FeedingIntent ↔ MetabolismComponent

**Contract**: Feeding transfers energy from plants to herbivore metabolism.

**Data Flow**:
- FeedingSystem reads: `PlantComponent::energy`, `FeedingIntent::reach/rate`, `MetabolismComponent::energy`
- FeedingSystem writes: `PlantComponent::energy` (decreased), `MetabolismComponent::energy` (increased)
- Guarantee: Energy never < 0; herbivore energy clamped to `max_energy`

**Energy Conservation**: Energy transferred atomically; no loss during transfer.

### BrainComponent ↔ ActuationComponent ↔ KinematicsComponent

**Contract**: Brain outputs actuation commands that drive movement.

**Data Flow**:
- BrainInferenceSystem writes: `ActuationComponent` (impulses, jump, eat)
- MotorSystem reads: `ActuationComponent`
- MotorSystem writes: `KinematicsComponent::accumulated_force` (applies impulses)
- MotorSystem resets: `ActuationComponent` (cleared after application)

**Synchronization**: Actuation applied before physics integration.

### GenomeHandleComponent ↔ All Components

**Contract**: Genome determines initial component values, components affect fitness.

**Data Flow**:
- Spawn: Genome → Component initial values
- Evolution: Component values → Fitness → Genome selection

**Future**: Genome encodes component parameters

## Performance Considerations

### Memory Layout

- **EnTT SoA**: Components stored in separate arrays (Structure of Arrays)
- **Cache efficiency**: Iterating over single component type is cache-friendly
- **Memory overhead**: Minimal (just component data, no vtable)

### Access Patterns

- **Single component**: `registry.get<Component>(entity)` - O(1)
- **Multiple components**: `registry.view<C1, C2>()` - O(N) iteration
- **Component addition**: `registry.emplace<Component>(entity)` - O(1) amortized

## Extension Points

### Adding New Components

1. Define struct in `components.h`
2. Systems query via `registry.view<NewComponent>()`
3. Entities attach via `registry.emplace<NewComponent>(entity, ...)`

### Component Validation

- **Current**: No validation (caller responsibility)
- **Future**: Validation systems could check component constraints

## Related Documentation

- [Core Simulation Module](./core_simulation.md) - How components are stored
- [Physics System Module](./physics_system.md) - Component usage example
- [Environment Module](./environment.md) - Plant and feeding component usage
- [Math Types](./math_types.md) - Vec3 type used by components

