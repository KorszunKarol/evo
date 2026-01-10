# Module: Phenotype Builder

## Overview

The phenotype builder transforms serialized genomes into runnable ECS entities. It materializes components (Transform, Kinematics, Collider, Metabolism, Brain, Actuation, etc.) from genome data and computes derived traits.

## Files

- `sim/include/evolution/genetics/phenotype_builder.h`
- `sim/src/genetics/phenotype_builder.cpp`
- `sim/include/evolution/genetics/derived_traits.h`
- `sim/src/genetics/derived_traits.cpp`

## Classes

### PhenotypeBuilder

**Purpose**: Stateless helper that builds ECS entities from genomes.

**Responsibilities**:
- Read genome data from storage
- Create/update ECS components
- Compute derived traits (mass, basal_rate, brain_cost)
- Validate phenotype constraints

**Public API**:

```cpp
class PhenotypeBuilder {
public:
    static PhenotypeBuildResult build(GenomeId id,
                                      entt::registry& registry,
                                      entt::entity entity,
                                      const GenomeStorage& storage) noexcept;
};
```

**Parameters**:

- `build(id, registry, entity, storage)`:
  - **Input**:
    - `id`: Genome identifier to instantiate
    - `registry`: Destination ECS registry
    - `entity`: Target entity handle (must be valid)
    - `storage`: Source genome storage
  - **Returns**: `PhenotypeBuildResult` with success flag, error message, and derived traits
  - **Complexity**: O(C + W) where C = component writes, W = brain weight count
  - **Side Effects**: Creates/overrides components on entity

**Component Creation**:

The builder creates the following components:

1. **TransformComponent**: Created only when absent; preserves caller-provided spawn location, otherwise initialized to `{0, 0, 0}`

2. **KinematicsComponent**:
   - `inverse_mass`: Computed from body mass density and volume
   - `linear_velocity`: `{0, 0, 0}`
   - `accumulated_force`: `{0, 0, 0}`
   - `linear_damping`: Default `0.05`
   - `restitution`: From body material (or default `0.0`)
   - `friction`: From body material (or default `0.6`)

3. **ColliderComponent**:
   - `type`: From genome body shape
   - `sphere`: Radius from body size.x (if Sphere)
   - `capsule`: Radius and half_height from body size (if CapsuleY)
   - `aabb`: Half extents from body size (if Box)
   - `offset`: `{0, 0, 0}` (local offset)
   - `material`: Friction/restitution from body (or defaults)

4. **MetabolismComponent**:
   - `energy`: Initialized to `max_energy`
   - `max_energy`: Derived from mass and brain complexity
   - `basal_rate`: Computed via `DerivedTraits::compute_basal_rate()`

5. **BrainComponent**:
   - `kind`: MLP or NEAT (from genome)
   - `input_count`: From genome brain definition
   - `output_count`: From genome brain definition
   - `update_interval`: `1.0 / update_rate_hz` (from genome)
   - `accum`: `0.0` (accumulator for rate control)

6. **ActuationComponent**:
   - `impulse_x`, `impulse_z`: `0.0`
   - `jump`: `false`
   - `eat`: `false`
   - `update_skip`: `0` (frames to skip)

7. **GenomeHandleComponent**:
   - `id`: Genome identifier

8. **ReproductionComponent**:
   - `cooldown`: `5.0` seconds
   - `timer`: `0.0`
   - `mate_radius`: `3.0` meters
   - `energy_threshold`: `120.0` (energy required to reproduce)

**Validation**:

- Body size bounds: Clamped to `[0.1, 10.0]` meters
- Mass density: Clamped to `[100.0, 5000.0]` kg/m³
- Brain weights: Validated against schema (no NaN/Inf)
- Component count: Ensures all required components present

**Error Handling**:

- Missing genome: Returns `{ok: false, msg: "Genome not found"}`
- Invalid body: Returns `{ok: false, msg: "Invalid body parameters"}`
- Invalid brain: Returns `{ok: false, msg: "Invalid brain structure"}`
- Registry errors: Propagated (EnTT exceptions)

### DerivedTraits

**Purpose**: Compute derived properties from genome data.

**Structure**:

```cpp
struct DerivedTraits {
    double mass{0.0};           // Computed from body volume × density
    double basal_rate{0.0};     // Metabolic consumption per second
    double brain_cost{0.0};      // Energy cost of brain complexity
};
```

**Computation**:

- **Mass**: `volume × mass_density`
  - Sphere: `(4/3) × π × radius³ × density`
  - CapsuleY: `π × radius² × (2×half_height + 4×radius/3) × density`
  - Box: `8 × half_extents.x × half_extents.y × half_extents.z × density`

- **Basal Rate**: `a × mass + b × brain_cost + c`
  - Default: `a = 0.5`, `b = 0.1`, `c = 0.5`

- **Brain Cost**: Complexity metric
  - MLP: `input_count + sum(hidden_layers) + output_count`
  - NEAT: `node_count + connection_count`

**Public API**:

```cpp
namespace evolution::genetics {

DerivedTraits compute_traits(const evolution::genome::Genome& genome) noexcept;

double compute_basal_rate(double mass, double brain_cost) noexcept;

}
```

## Data Contracts

### PhenotypeBuilder ↔ ECS Registry

**Contract**: Builder mutates registry to create components.

**Data Flow**:
- Input: Genome ID, entity handle
- Output: Components attached to entity

**Guarantees**:
- All required components created (or error returned)
- Component values deterministic (same genome → same components)
- Pre-existing `TransformComponent` (spawn pose) is preserved; all other generated components are overwritten deterministically

### PhenotypeBuilder ↔ GenomeStorage

**Contract**: Builder reads genomes from storage.

**Data Flow**:
- `storage.get(id)` → `const Genome*`
- Builder reads genome fields to populate components

**Guarantees**:
- Genome pointer valid during build
- Storage not mutated during build

## Usage Pattern

```cpp
GenomeStorage storage;
GenomeId genome_id = storage.create_random(12345);

entt::entity entity = registry.create();
auto result = PhenotypeBuilder::build(genome_id, registry, entity, storage);

if (!result.ok) {
    spdlog::error("Build failed: {}", result.msg);
    registry.destroy(entity);
} else {
    // Entity now has all components
    auto& transform = registry.get<TransformComponent>(entity);
    transform.position = {10.0, 5.0, 0.0};  // Set spawn location
    
    spdlog::info("Built phenotype: mass={}, basal_rate={}", 
                 result.traits.mass, result.traits.basal_rate);
}
```

## Performance Considerations

- **Component Creation**: O(C) where C = component count (~8 components)
- **Trait Computation**: O(1) for body, O(W) for brain (W = weight count)
- **Memory**: Minimal (stateless builder, temporary trait structs)

## Extension Points

### Adding New Components

1. Read genome field in `PhenotypeBuilder::build()`
2. Create component via `registry.emplace<NewComponent>(entity, ...)`
3. Update `PhenotypeBuildResult` if trait needed

### Custom Trait Formulas

1. Modify `DerivedTraits::compute_traits()`
2. Update `compute_basal_rate()` if metabolism formula changes
3. Ensure determinism (no random values in trait computation)

## Related Documentation

- [Genome Module](./genome.md) - Genome storage and operations
- [Components Module](./components.md) - ECS component definitions
- [Data Contracts](../data-contracts/genetics.md) - Inter-module data flow

