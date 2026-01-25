# Module: Social Behavior

## Overview

The Social Behavior module implements flocking, territoriality, pack hunting, and social signal generation for creatures. It translates brain outputs into coordinated group behaviors and provides social context inputs back to the brain.

## Files

- `/home/karolito/evolution/sim/include/evolution/sim/social_behavior_system.h`
- `/home/karolito/evolution/sim/src/systems/social_behavior_system.cpp`

## Component Definitions

### SocialSignalsComponent

**Purpose**: Provides computed social inputs to the brain based on nearby entities.

**Fields**:
- `cohesion` (double) - Average direction to nearby same-species creatures (flocking alignment)
- `alignment` (double) - Average velocity alignment with neighbors
- `separation` (double) - Vector pointing away from crowding neighbors
- `neighbor_density` (uint32_t) - Count of nearby creatures (crowding pressure)
- `territory_dist_norm` (double) - Normalized distance from territory center (0.0 = at center, 1.0 = at boundary)
- `intruder_density` (uint32_t) - Count of foreign species within territory
- `prey_dir` (Vec3) - Direction to nearest detected prey (normalized)
- `pack_density` (uint32_t) - Count of same-species pack members in range
- `intruder_density_near_prey` (uint32_t) - Intruders near detected prey (for coordinated hunting)

**Data Contract**:
- **Readers**: BrainInferenceSystem
- **Writers**: SocialBehaviorSystem
- **Lifetime**: Updated every tick

### TerritoryComponent

**Purpose**: Defines a creature's territorial region and ownership state.

**Fields**:
- `center` (Vec3) - Territory center position (meters, world space)
- `radius` (double) - Territory radius in meters
- `initialized` (bool) - Whether territory has been established

**Data Contract**:
- **Readers**: SocialBehaviorSystem, FeedingSystem (for spawning location)
- **Writers**: SocialBehaviorSystem (initialization on spawn), CreatureBehavior updates
- **Lifetime**: Persists while creature alive

### CarnivoreTag (Not Documented in components.md)

**Purpose**: Tag component identifying an entity as carnivorous.

**Structure**: Empty struct (marker component)

**Data Contract**:
- **Readers**: FeedingSystem, SocialBehaviorSystem
- **Writers**: PhenotypeBuilder (based on genome diet type)
- **Lifetime**: Component exists for creature lifetime

## Systems

### SocialBehaviorSystem

**Purpose**: Computes social signals, manages territorial behavior, coordinates pack hunting, and updates combat state.

**Algorithm**:

#### 1. Social Signal Computation

For each creature with SocialSignalsComponent:
```cpp
// Find neighbors within radius r
auto neighbors = creature_spatial_index.query_radius(position, social_radius);

// Compute cohesion (average direction to neighbors)
Vec3 cohesion = {0, 0, 0};
for (auto& neighbor : neighbors) {
    cohesion += normalize(neighbor.position - my.position);
}
cohesion = normalize(cohesion / neighbors.size());

// Compute alignment (average velocity direction)
Vec3 alignment = {0, 0, 0};
for (auto& neighbor : neighbors) {
    alignment += normalize(neighbor.velocity);
}
alignment = normalize(alignment / neighbors.size());

// Compute separation (vector away from crowding)
Vec3 separation = {0, 0, 0};
for (auto& neighbor : neighbors) {
    Vec3 diff = my.position - neighbor.position;
    if (diff.length() < separation_threshold) {
        separation += normalize(diff);
    }
}
separation = normalize(separation / neighbors.size());

// Write to SocialSignalsComponent
signals.cohesion = cohesion;
signals.alignment = alignment;
signals.separation = separation;
signals.neighbor_density = neighbors.size();
```

#### 2. Territorial Behavior

```cpp
// Check territory status
if (!territory.initialized) {
    // Establish territory around spawn location
    territory.center = transform.position;
    territory.radius = genome.territory_size;
    territory.initialized = true;
} else {
    // Check distance from territory center
    double dist = (transform.position - territory.center).length();
    territory_dist_norm = clamp(dist / territory.radius, 0.0, 1.0);
}

// Count intruders (different species within territory)
auto intruders = creature_spatial_index.query_radius(
    territory.center,
    territory.radius
);

int intruder_count = 0;
for (auto& entity : intruders) {
    if (get_species(entity) != get_species(my_entity)) {
        intruder_count++;
    }
}

signals.intruder_density = intruder_count;
```

#### 3. Pack Hunting Coordination

```cpp
// Detect prey via VisionComponent
auto prey = find_nearest_prey(vision.ray_hits);

if (prey.has_value()) {
    // Find nearby pack members
    auto pack_members = creature_spatial_index.query_radius(
        transform.position,
        pack_communication_radius
    );

    // Count pack members
    signals.pack_density = pack_members.size();

    // Check if intruders near prey (competition)
    auto intruders_near_prey = creature_spatial_index.query_radius(
        prey.position,
        competition_radius
    );

    int competing_packs = 0;
    for (auto& intruder : intruders_near_prey) {
        if (get_species(intruder) != my_species) {
            competing_packs++;
        }
    }

    signals.intruder_density_near_prey = competing_packs;
    signals.prey_dir = normalize(prey.position - transform.position);

    // Set combat target if pack member not already pursuing
    if (should_initiate_attack()) {
        combat.target = prey.entity;
        combat.pursuing = true;
    }
}
```

#### 4. Combat State Management

```cpp
// Update combat component based on brain outputs
if (actuation.attack && combat.attack_cooldown <= 0.0) {
    combat.target = vision.ray_hit_entities[0]; // Primary target
    combat.pursuing = true;
    combat.attack_timer = attack_duration;
    combat.attack_cooldown = attack_interval;
}

// Update timers
combat.attack_timer -= dt;
combat.attack_cooldown -= dt;

// Process combat resolution
if (combat.target != entt::null && in_range(combat.target)) {
    // Apply damage via physics system
    combat.damage_dealt = calculate_damage();
}
```

## Flocking Algorithm

The classic Reynolds Boids algorithm adapted for creature simulation:

**Separation Rule**: Steer to avoid crowding local flockmates.
**Alignment Rule**: Steer towards the average heading of local flockmates.
**Cohesion Rule**: Steer to move toward the average position (center of mass) of local flockmates.

**Combined Output**:
```cpp
Vec3 social_force =
    (separation * separation_weight) +
    (alignment * alignment_weight) +
    (cohesion * cohesion_weight);

actuation.impulse_x = social_force.x;
actuation.impulse_z = social_force.z;
```

## Territoriality Mechanisms

**Territory Establishment**:
- Random radius from genome (e.g., 5.0-20.0 meters)
- Centered on initial spawn position
- Marked initialized after first update

**Territory Enforcement**:
- Intruder detection radius = territory.radius × 1.5
- Aggression scales with intruder density
- High intruder count → combat mode initiated

**Territory Migration** (Planned):
- If food scarce in territory, territory center shifts toward better resources
- Old territory abandoned after migration timer

## Pack Hunting

**Pack Formation**:
- Pack members communicate via shared prey detection
- First hunter to detect prey "claims" it (sets combat.target)
- Subsequent hunters follow target without independent detection

**Pack Attack Bonus**:
- Multiple attackers on same prey → damage multiplier
- Damage divided among pack members on kill
- Biomass shared proportionally

## Integration

### System Execution Order

1. **VisionSystem** - Detects prey, neighbors
2. **BrainInferenceSystem** - Generates social/movement outputs
3. **SocialBehaviorSystem** - Computes social signals, manages territory, coordinates hunting
4. **MotorSystem** - Applies social forces to movement
5. **PhysicsSystem** - Detects combat collisions
6. **CombatResolutionSystem** (Planned) - Resolves combat

### Data Flow

```
CreatureSpatialIndex (neighbors)
    ↓
SocialBehaviorSystem (compute social signals)
    ↓
SocialSignalsComponent (cohesion, alignment, separation, etc.)
    ↓
BrainInferenceSystem (use signals as inputs)
    ↓
ActuationComponent (social forces applied to movement)
    ↓
MotorSystem (physics forces)
```

## Configuration Parameters

**Social Radii** (genome traits):
- `social_radius` (double) - Range for flocking (typical: 5-15m)
- `separation_threshold` (double) - Minimum comfortable distance (typical: 1-3m)
- `pack_communication_radius` (double) - Range for pack coordination (typical: 20-50m)

**Territory Parameters**:
- `territory_size` (double) - Radius of territory (typical: 10-30m)
- `territory_aggression` (double) - Willingness to defend territory [0.0-1.0]

**Flocking Weights** (genome traits):
- `separation_weight` (double) - Importance of avoiding crowding
- `alignment_weight` (double) - Importance of matching neighbor velocity
- `cohesion_weight` (double) - Importance of staying near group center

## Related Documentation

- [Components Module](./components.md) - SocialSignalsComponent, TerritoryComponent, CarnivoreTag
- [Vision Module](./vision.md) - Prey detection and raycasting
- [Combat Module](./combat.md) - Combat state management
- [Spatial Indexing](./spatial_indexing.md) - CreatureSpatialIndex for neighbor queries
