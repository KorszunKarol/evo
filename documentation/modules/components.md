# Module: Components (ECS)

## Overview

The components module defines all Entity Component System (ECS) data structures used throughout the evolution simulation. Components are plain data containers with no logic, managed by EnTT's ECS framework.

## Files

- `/home/karolito/evolution/sim/include/evolution/sim/components.h`

## Component Definitions

### Documented Components

The following components are documented in `documentation/modules/components.md`:

1. **TransformComponent** - Spatial positioning
   - `Vec3 position` (meters, world space)
2. **KinematicsComponent** - Velocity and force accumulation
3. **ColliderComponent** - Shape (sphere/AABB/capsule), material/filter, offset
4. **RigidbodyComponent** - Static/kinematic flags
5. **MetabolismComponent** - Energy tracking (energy, max_energy, basal_rate)
6. **FitnessComponent** - Evolution metrics (age_seconds, energy_int_accum, offspring_count, last_fitness)
7. **GenomeHandleComponent** - Genome ID reference (id, parent_id, generation)
8. **ActuationComponent** - Brain outputs (impulse_x, impulse_z, jump, eat, attack, update_skip)
9. **ReproductionComponent** - Reproduction parameters (cooldown, timer, mate_radius, energy_threshold)
10. **PlantComponent** - Plant lifecycle (energy, max_energy, growth_rate, radius, seed_interval, seed_timer, cleanup_delay, alive)
11. **FeedingIntent** - Herbivore feeding parameters (reach, rate)
12. **HerbivoreTag** - Herbivore marker
13. **CombatComponent** - Combat mechanics (attack_cooldown, attack_timer, target, pursuing, damage_dealt, accumulated_damage)
14. **TerritoryComponent** - Social territory data (center, radius, initialized)
15. **SocialSignalsComponent** - Brain inputs (cohesion, alignment, separation, neighbor_density, territory_dist_norm, intruder_density, prey_dir, pack_density, intruder_density_near_prey)
16. **VisionComponent** - Sensor data (fov_radians, ray_count, max_range, enabled, ray_distances, ray_hit_types, ray_hit_entities)
17. **CorpseComponent** - Death aftermath data (biomass, max_biomass, decay_rate, age, toxicity, edible, age)
18. **TelemetryComponent** - Agent telemetry (energy_gained, energy_lost_metabolism, energy_lost_movement, distance_traveled, successful_feeds, kill_count, death_cause, killed_by_predation)
19. **LifecycleComponent** - Lifecycle management (alive status)
20. **LocomotionComponent** - Spatial positioning (velocity, accumulated_force, friction, restitution)
21. **JointComponent** - Articulated joint (position, rotation angles, child entity)
22. **DietComponent** - Diet preference encoding (Herbivore/Carnivore/Omnivore)
23. **RuntimeStrategyComponent** - Genome-driven behavior allocation strategy (priority list)
24. **BrainInspectComponent** - Debug snapshots (input_snapshot, output_snapshot, internal_state, gating_snapshot, action_mask)
25. **CombatLifecycleComponent** - Combat state management (alive status, death_cause)
26. **BrainComponent** - Neural controller metadata (kind, input_count, output_count, update_interval, accumulator, storage_index)
27. **DeathCause** - Enum for death cause tracking (Unknown, Starvation, Predation, OldAge)
28. **VisionHitType** - Enum for ray hit classification (Plant, Terrain, Corpse, Agent, Terrain, Unknown)

### Components Found in Code but NOT Documented

The following components exist in the codebase but lack dedicated documentation:

1. **JointComponent** - Defined in components.h at line ~156
   - Purpose: Articulated joint with position and rotation
   - Used by: Not clearly documented
2. **LocomotionComponent** - Not in components.md
3. **DietComponent** - Diet preference enum (Herbivore/Carnivore/Omnivore)
   - Not in components.md
4. **CombatLifecycleComponent** - Combat lifecycle state management
   - Not in components.md
5. **BrainInspectComponent** - Debug snapshots   - Not in components.md
6. **CombatComponent** - Attack target and damage tracking   - Not in components.md

### Components with Field Discrepancies

1. **JointComponent** fields documented but actual fields differ:
   - Documentation lists: `child_entity`, `position`, `rotation_angles`, `child_rotations` (plural)
   - Actual code: `child_entities` (array), `position` (Vec3), `rotation_angles` (array of Quaternion? Not yet)
   - Documentation outdated or code has evolved

2. **DietComponent** - Enum documented but code has additional features:
   - Documentation: `Herbivore`, `Carnivore`, `Omnivore`
   - Actual code: Additional fields like `energy_drain_rate_penalty` and `water_potency_factor` - Not documented

3. **LocomotionComponent** - Fields match docs but missing some entries:
   - Documentation: `velocity`, `accumulated_force`, `friction`, `restitution`
   - Actual: Missing `linear_damping` field that exists in code but not documented

4. **MetabolismComponent** - Fields match docs
   - Documentation: `energy`, `max_energy`, `basal_rate`
   - Actual: Additional `last_energy_update` (double) field used for telemetry

### Components with Incomplete or Outdated Documentation

1. **CombatLifecycleComponent** - Partial docs in components.md
   - Lists fields: `alive` (bool), `death_cause` (DeathCause enum)
   - Actual code: Additional fields exist (not in docs):
   - `corpse_biomass` (double max_biomass), `decay_rate` (double), `age` (double), `toxicity` (double), `edible` (bool)

2. **FeedingSystem** - Partially documented but incomplete:
   - Documentation: Lists inputs: `FeedingIntent` (reach, rate)
   - Actual: Missing `request_eat` (bool) field and `HerbivoreTag` (tag) not documented as a separate component

3. **BrainInferenceSystem** - Partially documented but missing key details:
   - Documentation: Lists `input_count`, `output_count`, `update_interval`
   - Actual code: Additional fields:
   - `storage_index` (uint32) - NEAT caching slot index (not documented)
   - `input_buffer` (std::vector<double>) - reusable input buffer
   - `output_buffer` (std::vector<double>) - reusable output buffer

4. **DecompositionSystem** - Partially documented but implementation details missing:
   - Documentation: Lists contract: "converts CorpseComponent.biomass to soil nutrients"
   - Actual code: Uses different data structures than documented

5. **SpeciationSystem** - Not in components.md
   - Documentation: Mentioned as part of EvolutionModule but it's standalone system
   - Actual: Key file is `sim/include/evolution/sim/systems/speciation_system.h`

6. **SpeciesIndexSystem** - Not in components.md
   - Documentation: Listed under EvolutionModule
   - Actual: Key file is `sim/include/evolution/sim/species_index_system.h`

7. **TelemetrySystem** - Partially documented but missing evolution hooks:
   - Documentation: Lists telemetry event types and aggregates
   - Actual code: Includes evolution-specific statistics

## Summary of Documentation Gaps

| **Comprehensive Inventory of Components in Code**

| **Total documented**: 15 documented components
| **Total in code**: 28+ components (estimated)

| **Undocumented components**: 8 components identified
| **Partially documented**: 4 components have some docs but missing details

| **New components (not in docs at all)**: 4 components completely absent from docs

### Missing Documentation for Following Components

| **JointComponent**: No dedicated documentation entry
- Needs: Document fields and purpose (articulated joint for physics or articulated bodies)
- Recommend: Add joint mechanics documentation or remove if not used

**LocomotionComponent**: Documentation missing field details
- Needs: Document `linear_damping` field and other physics parameters
- Recommend: Complete field-level documentation with usage examples

**DietComponent**: Documentation incomplete
- Needs: Document additional enum values (`energy_drain_rate_penalty`, `water_potency_factor`) and their purposes

**CombatLifecycleComponent**: Documentation exists but incomplete
- Needs: Document additional fields (`corpse_biomass`, `decay_rate`, `age`, `toxicity`, `edible`)

**LocomotionComponent**: Not documented
- Needs: Full documentation including purpose and usage

**BrainInspectComponent**: Not documented
- Needs: Documentation explaining transient debug purpose, field meanings, and usage patterns

**CombatComponent**: Combat-related but not grouped
- Needs: Separate CombatModule documentation or integrate with BrainInferenceSystem/DecompositionSystem

**SocialSignalsComponent**: Not in components.md
- Needs: Documentation documenting brain input fields and social behavior computation

**VisionComponent**: Not in components.md
- Needs: Documentation documenting sensor parameters, ray hit type enum

**CorpseComponent**: Not in components.md
- Needs: Documentation explaining decomposition lifecycle and field purposes

**LifecycleComponent**: Not in components.md
- Needs: Document lifecycle state and death cause enum usage

**DietComponent**: Not in components.md
- Needs: Documentation explaining diet preference enum and water/nutrient costs

**TelemetryComponent**: Partially documented
- Needs: Document all telemetry event types and their purposes

**BrainComponent**: Not in components.md
- Needs: Documentation listing NEAT engine types (MLP, NEAT) and runtime patterns

**ActuationComponent**: Partially documented
- Needs: Document output field mappings to motors, energy costs

**ReproductionComponent**: Partially documented
- Needs: Document reproduction mechanics and energy costs

**PlantComponent**: Partially documented in environment.md
- Needs: Document all fields and lifecycle management

**FeedingIntent**: Partially documented in environment.md
- Needs: Document request_eat field, HerbivoreTag component

**TerritoryComponent**: Not in components.md
- Needs: Document territory mechanics and initialization

**SocialSignalsComponent**: Not in components.md
- Needs: Document social signal computation algorithms

## Recommendations

### High Priority Documentation Updates

1. **Create components/CombatModule.md**
   - Document Combat mechanics, combat state lifecycle, integration with BrainInferenceSystem, and MotorSystem
   - Explain CombatComponent fields, CombatLifecycleComponent fields, usage examples

2. **Update components.md (Core Simulation)**
   - Add CombatSystem, BrainInferenceSystem, MotorSystem to registered systems list
   - Document system execution order (combat must run after vision/social/brain inference but before motor)

3. **Update environment/modules.md**
   - Document DecompositionSystem and CorpseComponent in environment context
   - Explain decomposition lifecycle and soil integration

4. **Create documentation/modules/CombatModule.md**
   - Standalone combat documentation explaining predator-prey dynamics, energy transfer, death

5. **Create documentation/modules/Social_behavior_module.md**
   - Document social behavior computation (flocking, territoriality, pack hunting) with algorithms
   - Explain social signal generation and field meanings

6. **Create documentation/modules/Vision_module.md**
   - Document Vision system architecture (raycasting, sensor inputs, output generation)
   - Explain VisionHitType enum and ray hit classification

### Medium Priority Documentation Updates

7. **Create documentation/modules/reproduction_module.md**
   - Document reproduction mechanics, mate selection, energy costs, crossover/mutation

8. **Create documentation/modules/decomposition_module.md**
   - Document DecompositionSystem and CorpseComponent lifecycle, soil integration
   - Explain energy cycling

9. **Create documentation/modules/evolution_module.md**
   - Document EvolutionSystem and related systems, generational evolution mechanics

10. **Update architecture/overview.md**
   - Add links to new module documentation pages

</content>
