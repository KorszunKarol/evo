# Testing Guide

This document describes the local test workflows for the Evolution simulation.

## Quick Start

```bash
# Build tests
cmake --build build --target sim_tests

# Run all tests (fast - ~1-2 seconds)
ctest --test-dir build --output-on-failure
```

## Test Categories

### Smoke Tests (4 tests, ~0.1s)

Fast sanity checks to run before committing. Validates core determinism and basic functionality.

```bash
ctest --test-dir build -R "Mutation.Determinism|Crossover.Determinism|RngSeedDerivation.DeterministicSameInputs|IntegrationSystemOrdering.DeterministicRun" --output-on-failure
```

**Tests:**
- `Mutation.Determinism` - Mutation operations are deterministic
- `Crossover.Determinism` - Crossover operations are deterministic
- `RngSeedDerivation.DeterministicSameInputs` - RNG seeding is reproducible
- `IntegrationSystemOrdering.DeterministicRun` - Full simulation runs are deterministic

### Integration Tests (17 tests, ~0.2s)

Cross-system behavior validation. Tests system interactions and data flow between components.

```bash
ctest --test-dir build -R "IntegrationSystemOrdering|BiomeWaterIntegration|PlantSpeciesMapping" --output-on-failure
```

**Test suites:**
- `IntegrationSystemOrdering.*` - System execution order and data dependencies
- `BiomeWaterIntegration.*` - Biome and water system interactions
- `PlantSpeciesMapping.*` - Plant species placement rules across biomes

### Performance Tests (6 tests, ~0.5s)

Performance regression detection. Validates that core systems scale as expected.

```bash
ctest --test-dir build -R "PerformanceCadence" --output-on-failure
```

**Tests:**
- Fitness update scaling
- Species indexing performance
- Cadence window timing stability
- Large population handling
- Empty registry overhead
- Memory usage bounds

### All Tests (72 tests, ~1-2s)

Complete test suite - run this before pushing.

```bash
ctest --test-dir build --output-on-failure
```

## Running Specific Tests

Run a single test by name:

```bash
ctest --test-dir build -R "TestSuite.TestName" --output-on-failure
```

List tests matching a pattern without running:

```bash
ctest --test-dir build -R "Pattern" -N
```

## Known Issues

**Test #70: `IntegrationSystemOrdering.FeedingAfterPlantGrowth`**
- Status: Fails deterministically (not a flake)
- Issue: Test logic bug - FeedingSystem not triggering as expected
- Impact: Does not indicate simulation determinism problems
- Tracked in: `.sisyphus/notepads/deep-simulation-test-upgrade/issues.md`

**Test #77: `PerformanceCadence.CadenceWindowDoesNotSpike`**
- Status: Flaky - timing threshold is too tight
- Issue: Performance test occasionally fails on slower machines
- Impact: Not a correctness or determinism issue
- Tracked in: `.sisyphus/notepads/deep-simulation-test-upgrade/issues.md`

## Test Infrastructure

- **Framework**: GoogleTest
- **Discovery**: `gtest_discover_tests()` in CMakeLists.txt
- **Fixtures**: `tests/sim/test_fixtures.{h,cpp}`
- **State hashing**: `hash_entity_state(registry)` for determinism validation
- **Build target**: `sim_tests` (builds test binary without building full application)

## Determinism Validation

All tests use deterministic seeding and fixed-timestep simulation. State hashing (`hash_entity_state()`) provides reproducibility guarantees:

- Same seed + same config → identical state hash
- Multi-run tests validate consistency across runs
- No wall-clock dependencies (uses `simulation_time()` only)
- No non-deterministic RNG (explicit seeding via `rng_seed_derivation.h`)

## Build Notes

The main application (`sim_app`) may have compilation errors during development, but tests build independently:

```bash
# If full build fails, build just the tests
cmake --build build --target sim_tests
```

Tests can run successfully even if the main application doesn't compile.
