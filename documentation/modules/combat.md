# Module: Combat

## Overview

The Combat module handles predator-prey interactions, territorial disputes, and damage mechanics within the ecosystem simulation.

## Files

- `/home/karolito/evolution/sim/include/evolution/sim/combats_component.h`
- `/home/karolito/evolution/sim/include/evolution/sim/combats_lifecycle_component.h`
- `/home/karolito/evolution/sim/include/evolution/sim/social_behavior_system.h`

## Component Definitions

### CombatComponent

**Purpose**: Tracks combat state for carnivorous creatures during hunting and territorial disputes.

**Fields**:
- `attack_cooldown` (double) - Time before next attack can be initiated
- `attack_timer` (double) - Progress tracking for current attack animation
- `target` (entt::entity) - Current prey or opponent being targeted
- `pursuing` (bool) - Whether actively chasing target
- `damage_dealt` (double) - Accumulated damage during current combat
- `accumulated_damage` (double) - Total damage received from all attacks

**Data Contract**:
- **Readers**: SocialBehaviorSystem, MotorSystem
- **Writers**: BrainInferenceSystem (sets attack intent), PhysicsSystem (collision detection)
- **Lifetime**: Persists while creature is alive and engaged in combat

### CombatLifecycleComponent

**Purpose**: Manages combat lifecycle states (alive, fighting, dying, dead) and tracks combat statistics.

**Fields**:
- `alive` (bool) - Current combat viability state
- `death_cause` (DeathCause enum) - Cause of death (Starvation, Predation, OldAge)
- `corpse_biomass` (double) - Total biomass available upon death
- `decay_rate` (double) - Rate at which corpse decomposes
- `age` (double) - Time since death
- `toxicity` (double) - Chemical toxicity level (affects scavenging)
- `edible` (bool) - Whether corpse can be consumed by scavengers

**Data Contract**:
- **Readers**: DecompositionSystem, TelemetrySystem
- **Writers**: SocialBehaviorSystem (on death), FeedingSystem (on consumption)
- **Lifetime**: Created on entity death, destroyed after decomposition

## Systems

### SocialBehaviorSystem

**Purpose**: Implements territorial behavior, pack dynamics, and social interactions (flocking, hunting coordination, territorial defense).

**Responsibilities**:
- Update combat state (CombatComponent) based on brain outputs
- Manage territorial disputes and intruder responses
- Coordinate pack hunting behavior (multiple predators attacking same prey)
- Generate social signal inputs for brain (SocialSignalsComponent)

**Component Requirements**:
- Reads: `BrainComponent`, `ActuationComponent`, `TransformComponent`, `CombatComponent`, `TerritoryComponent`
- Writes: `CombatComponent`, `SocialSignalsComponent`, `ActuationComponent`

**Complexity**: O(N) where N = creatures with CombatComponent

**Thread Safety**: Not thread-safe

### CombatResolutionSystem (Planned)

**Purpose**: Resolves combat outcomes, applies damage, manages health, triggers death when thresholds reached.

**Data Flow**:
```
BrainInferenceSystem outputs attack intent
    ↓
SocialBehaviorSystem validates and initiates attack
    ↓
PhysicsSystem collision detection
    ↓
CombatResolutionSystem applies damage, updates health
    ↓
If health <= 0 → Create CorpseComponent, mark dead
```

## Integration

### Combat System Execution Order

1. **VisionSystem** - Detects prey, intruders, pack members
2. **BrainInferenceSystem** - Generates attack/flee/social outputs
3. **SocialBehaviorSystem** - Interprets brain outputs, updates CombatComponent
4. **MotorSystem** - Executes movement toward target
5. **PhysicsSystem** - Detects collisions, contacts
6. **CombatResolutionSystem** (Planned) - Applies damage, manages health
7. **DecompositionSystem** - Processes corpses into soil nutrients

### Combat Mechanics

**Attack Sequence**:
1. Predator detects prey via VisionSystem
2. Brain decides to attack (ActuationComponent.attack = true)
3. SocialBehaviorSystem validates cooldown, sets target, initiates pursuit
4. When in range, collision detected by PhysicsSystem
5. Damage applied based on predator size, weapon traits (if implemented)
6. Accumulated damage tracked in CombatComponent
7. If prey health depleted, CorpseComponent created

**Territorial Defense**:
1. Intruder detected within TerritoryComponent.radius
2. SocialBehaviorSystem checks territory ownership and threat level
3. If defending, intruder marked as target in CombatComponent
4. Aggression level determined by pack density (SocialSignalsComponent.intruder_density)

**Pack Hunting**:
1. Pack members detect same prey via SocialSignalsComponent.pack_density
2. SocialBehaviorSystem coordinates attack (same target)
3. Damage multiplied by number of attackers
4. Biomass divided among pack members upon kill

## Energy Costs

- **Pursuit Cost**: Additional metabolic drain when pursuing (MetabolismComponent.basal_rate × pursuit_multiplier)
- **Attack Cost**: Energy spike during attack (deducted from MetabolismComponent.energy)
- **Combat Stress**: Reduced feeding efficiency after combat (temporary multiplier)

## Related Documentation

- [Components Module](./components.md) - CombatComponent, CombatLifecycleComponent definitions
- [Brain Module](./brain.md) - Brain outputs for attack/flee decisions
- [Environment Module](./environment.md) - Decomposition of corpses
- [Physics Module](./physics_system.md) - Collision detection
