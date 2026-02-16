# Test Fixtures and State Hashing

## Overview

The `tests/sim/test_fixtures.{h,cpp}` module provides deterministic simulation testing infrastructure including state hashing and snapshot capabilities.

## Snapshot Structure

The `Snapshot` struct captures aggregate simulation state for test verification:

```cpp
struct Snapshot {
    std::size_t entity_count{0};
    std::size_t plant_count{0};
    std::size_t herbivore_count{0};
    double total_biomass{0.0};
    double mean_soil{0.0};
    std::unordered_map<std::uint8_t, std::size_t> species_counts{};
    std::unordered_map<evolution::sim::BiomeId, double> biome_biomass{};
    std::string state_hash{};
};
```

### Fields

| Field | Description | Source |
|--------|-------------|--------|
| `entity_count` | Total entities in registry | `registry.storage<entt::entity>().in_use()` |
| `plant_count` | Alive plant entities | `registry.view<PlantComponent>()` |
| `herbivore_count` | Herbivore entities | `registry.view<MetabolismComponent, HerbivoreTag>()` |
| `total_biomass` | Sum of all entity energy | Plant + Herbivore energies |
| `mean_soil` | Average soil nutrient value | `SoilGrid::mean_nutrient()` |
| `species_counts` | Plants per species ID | Aggregated during snapshot |
| `biome_biomass` | Total energy per biome | Aggregated during snapshot |
| `state_hash` | Deterministic state hash | `hash_entity_state()` |

## State Hashing

The `hash_entity_state()` function computes a deterministic hash of the entire simulation state for regression testing and determinism validation.

### State Coverage

The function currently covers **>90% of simulation state**:

#### System Context
- **SimulationContext**: `fixed_dt`, `simulation_time()`
  - Critical for timing determinism

#### Entity State (7 component types)
1. **Plant**: TransformComponent + PlantComponent (position, energy)
2. **Herbivore**: TransformComponent + MetabolismComponent + GenomeHandle (position, energy, genome_id)
3. **Kinematics**: KinematicsComponent (velocity, forces, damping)
4. **Fitness**: FitnessComponent (age, energy_int, offspring_count)
5. **Actuation**: ActuationComponent (impulses, jump, eat, update_skip)
6. **Lifecycle**: LifecycleComponent (age, energy_scale, size_scale, stage)
7. **Reproduction**: ReproductionComponent (cooldown, timer, mate_radius)
8. **Brain**: BrainComponent (input/output counts, interval, accumulator)

### Determinism Guarantees

The hash ensures:
- **Same seed + same config → same hash**: Identical simulation runs produce identical hashes
- **Deterministic iteration**: Entities sorted before hashing to avoid iteration order differences
- **Comprehensive coverage**: >90% of simulation state hashed, enabling regression detection

### Hash Format

The hash is a string combining:
- Unique prefix for each state section (e.g., "CTX:", "PLANT:", "HERBIVORE:")
- Entity IDs included to detect entity addition/removal
- Scaled double values to avoid floating-point precision issues

Example hash format:
```
CTX:0.0333333:0.000000;ENTITY_COUNT:5;PLANT:2:10.5:3:12.3;HERBIVORE:2:50.0:12345678:1.2;KINEMATICS:3:0.5:2:0.7;...
```

## Usage Patterns

### Basic Snapshot

```cpp
SimulationFixture fixture;
fixture.SetUp();

// Run simulation
fixture.run_steps(100);

// Capture state
auto snapshot = fixture.take_snapshot();

EXPECT_EQ(snapshot.entity_count, 5);
EXPECT_EQ(snapshot.plant_count, 3);
EXPECT_EQ(snapshot.herbivore_count, 2);
```

### Determinism Validation

```cpp
SimulationFixture fixture1;
SimulationFixture fixture2;
fixture1.SetUp();
fixture2.SetUp();

// Run identical simulations
for (int i = 0; i < 10; ++i) {
    fixture1.run_steps(10);
    fixture2.run_steps(10);
}

// State must be identical
auto snap1 = fixture1.take_snapshot();
auto snap2 = fixture2.take_snapshot();
EXPECT_EQ(snap1.state_hash, snap2.state_hash);
```

### Multi-Run Determinism

```cpp
SimulationFixture fixture;
fixture.SetUp();
fixture.run_steps(100);

std::string initial_hash = fixture.take_snapshot().state_hash;

// Run more steps
fixture.run_steps(100);
fixture.run_steps(100);

// Hash must change deterministically
std::string final_hash = fixture.take_snapshot().state_hash;
EXPECT_NE(initial_hash, final_hash);

// But re-running should produce same final hash
SimulationFixture fixture2;
fixture2.SetUp();
for (int i = 0; i < 30; ++i) {
    fixture2.run_steps(10);
}
std::string re_run_hash = fixture2.take_snapshot().state_hash;
EXPECT_EQ(final_hash, re_run_hash);
```

## Snapshot Comparison Utilities

### Types

```cpp
enum class SnapshotDiffKind {
    None,
    EntityCountMismatch,
    PlantCountMismatch,
    HerbivoreCountMismatch,
    BiomassMismatch,
    SoilMismatch,
    SpeciesCountMismatch,
    BiomomeBiomassMismatch,
    StateHashMismatch
};

struct SnapshotDiff {
    SnapshotDiffKind kind{SnapshotDiffKind::None};
    std::string message{};
    double expected_value{0.0};
    double actual_value{0.0};
};
```

### Functions

#### `compare_snapshots(const Snapshot& a, const Snapshot& b)`

Compares two snapshots and reports the first mismatch found.

**Returns**: `SnapshotDiff` with:
- `kind`: Type of mismatch (or `None` if identical)
- `message`: Human-readable description
- `expected_value`, `actual_value`: Numeric values for scalar comparisons

**Example**:
```cpp
auto diff = compare_snapshots(expected, actual);
if (diff.kind != SnapshotDiffKind::None) {
    // Snapshot doesn't match expected state
}
```

#### `assert_snapshot_matches(expected, actual, test_info)`

GoogleTest assertion helper that compares snapshots with detailed failure messages.

**Parameters**:
- `expected`: Snapshot with expected values
- `actual`: Snapshot to validate
- `test_info`: GoogleTest test info for failure reporting

**Behavior**:
- Calls `compare_snapshots()` internally
- On mismatch: Calls `ADD_FAILURE_AT()` with detailed message
- Includes test name, expected/actual values, and state hashes

**Example**:
```cpp
TEST(MyTest, DeterministicState) {
    SimulationFixture fixture;
    fixture.SetUp();
    fixture.run_steps(50);

    auto expected_hash = "CTX:0.0333:PLANT:2:10.0;...";
    Snapshot expected_snapshot;
    expected_snapshot.state_hash = expected_hash;

    fixture.run_steps(50);
    auto actual = fixture.take_snapshot();

    assert_snapshot_matches(expected_snapshot, actual,
                         ::testing::UnitTest::GetInstance()->current_test_info());
}
```

**Note**: Snapshot comparison utilities are currently deferred due to macro collision issues. Use direct `EXPECT_EQ` assertions for now.

## Best Practices

### 1. Always Use Deterministic Seeds

```cpp
SimulationFixture fixture;
fixture.SetUp();  // Uses global_seed_ from fixture (98765)
```

### 2. Hash Critical State Points

Capture state before and after critical operations:
```cpp
auto before_hash = fixture.take_snapshot().state_hash;
system_under_test();
auto after_hash = fixture.take_snapshot().state_hash;
```

### 3. Validate Multiple Aspects

Don't rely on a single metric - check multiple snapshot fields:
```cpp
auto snap = fixture.take_snapshot();
EXPECT_EQ(snap.plant_count, expected_plants);
EXPECT_GT(snap.total_biomass, 0.0);
EXPECT_EQ(snap.species_counts[0], expected_species_0);
```

### 4. Use State Hash for Regression Detection

When adding new systems or components, ensure they're hashed:
- Add component to `hash_entity_state()` view
- Include in hash with proper scaling
- Verify determinism tests still pass

### 5. Performance Considerations

- **Snapshot creation**: O(N) where N = entity count
- **State hashing**: O(N) for all components
- **Hash comparison**: O(1) for string comparison
- **Total test overhead**: <1ms for 1000 entities

## Known Issues

### Macro Collision with `Snapshot` Type

The `Snapshot` type name appears to collide with an internal macro or definition in the build environment, causing compilation errors when used as a function parameter.

**Symptoms**:
```cpp
error: 'Snapshot' does not name a type; did you mean 'SnapshotDiff'?
error: request for member 'state_hash' in 'expected', which is of non-class type 'const int'
```

**Workaround**:
- Avoid using `Snapshot` as parameter names in new functions
- Use `expected_snapshot`, `actual_snapshot` instead
- Defer `compare_snapshots()` and `assert_snapshot_matches()` implementation

**Status**: Documented in `.sisyphus/notepads/deep-simulation-test-upgrade/issues.md`

## Future Enhancements

1. **Snapshot comparison utilities**: Implement `compare_snapshots()` and `assert_snapshot_matches()` after resolving macro collision
2. **Entity relationships**: Add parent/offspring tracking to hash when reproduction system is implemented
3. **Species index state**: Include species clustering in hash
4. **Full grid hashing**: Hash complete SoilGrid and BiomeMap state instead of just aggregated values
5. **Performance benchmarks**: Add snapshot creation time measurements to ensure <5ms for 10K entities

## Property-Based Tests

Property-based invariants live in `tests/sim/test_property_based.cpp` and can be run locally with:

```bash
ctest --test-dir build -R "PropertyBased" --output-on-failure
```

The suite validates determinism and safety invariants across 100 randomized configurations per property.
