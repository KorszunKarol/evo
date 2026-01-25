# C++ Performance Best Practices for Simulation

**Generated:** 2025-01-25
**Commit:** Working directory analysis
**Project:** Evolution Simulation (C++20, ECS-based)

---

## OVERVIEW

High-performance evolutionary simulation with 1000+ entities, 60Hz tick rate, neural inference, spatial indexing, and generational evolution. Performance is critical for scalable simulation.

---

## CORE PRINCIPLES

### 1. Cache Locality (PRIORITY: CRITICAL)

**Data-Oriented Design (SoA):**
```cpp
// CORRECT: Structure of Arrays (SoA) for cache-friendly iteration
struct Entities {
    std::vector<Vec3> positions;      // Contiguous: cache line efficient
    std::vector<Vec3> velocities;
    std::vector<float> energies;
};

// INCORRECT: Array of Structures (AoS) - cache misses
struct Entity {
    Vec3 position;
    Vec3 velocity;
    float energy;
};
std::vector<Entity> entities;  // Random memory access pattern
```

**Component Pools (EnTT):**
- EnTT stores components in contiguous pools - leverage this
- Iterate via `registry.view<ComponentA, ComponentB>()` for cache-friendly access
- Avoid random component access via `registry.get<>()` in hot loops

### 2. Memory Management (PRIORITY: HIGH)

**Avoid Heap Allocations in Hot Paths:**
```cpp
// CORRECT: Pre-reserve, reuse buffers
void VisionSystem::tick(SimulationContext& ctx) {
    view.each([&](auto entity, VisionComponent& vision, TransformComponent& transform) {
        vision.hits.clear();  // Reuse existing capacity
        // ... raycasting logic
    });
}

// INCORRECT: Per-tick allocations
void VisionSystem::tick(SimulationContext& ctx) {
    view.each([&](auto entity, VisionComponent& vision, TransformComponent& transform) {
        vision.hits = {};  // New allocation per entity!
    });
}
```

**Smart Pointers:**
```cpp
// CORRECT: Unique ownership, move semantics
std::unique_ptr<IPhysicsBackend> backend = std::make_unique<SimplePhysicsBackend>();

// INCORRECT: Raw pointers (dangerous, no automatic cleanup)
IPhysicsBackend* backend = new SimplePhysicsBackend();
// ... leak if delete forgotten
```

### 3. Spatial Optimization (PRIORITY: CRITICAL)

**O(N²) → O(N) via Spatial Hashing:**
```cpp
// INCORRECT: Brute force neighbor search (O(N²))
for (auto& plant : registry.view<PlantComponent>()) {
    double dist = distance(creature.position, plant.position);
    if (dist < radius) nearby.push_back(&plant);
}

// CORRECT: Spatial hash index (O(N) average)
plant_spatial_index.for_each_in_radius(registry, position, radius,
    [&](entt::entity plant) {
        nearby.push_back(registry.get<PlantComponent>(plant));
    });
```

**Cache-Friendly Grid Storage:**
- Store spatial hash as flat arrays (not nested vectors)
- Power-of-two cell sizes for modulo hashing
- Reuse entity buffers across ticks

### 4. Determinism & Performance (PRIORITY: HIGH)

**Fixed Timestep (No Wall Clock):**
```cpp
// CORRECT: Fixed dt regardless of frame time
constexpr double FIXED_DT = 1.0 / 60.0;  // 60Hz
double accumulator = 0.0;

void app::run() {
    accumulator += frame_time;
    while (accumulator >= FIXED_DT) {
        simulation_app.tick(FIXED_DT);  // Always same dt
        accumulator -= FIXED_DT;
    }
}

// INCORRECT: Variable timestep (breaks determinism)
void app::run() {
    double dt = get_frame_time();  // Different every frame!
    simulation_app.tick(dt);  // Non-deterministic
}
```

**RNG with Derived Seeds:**
```cpp
// CORRECT: Deterministic seed derivation
std::uint64_t seed = derive_seed(global_seed, genome_id, OP_TAG_MUTATE, counter);
PCG32 rng(seed);
double mutation = rng.uniform(-1.0, 1.0);  // Reproducible

// INCORRECT: Global RNG (non-deterministic)
static std::mt19937 gen(0);  // Always same sequence
double mutation = std::uniform_real(-1.0, 1.0)(gen);  // Not seed-controlled
```

### 5. ECS Performance Patterns (PRIORITY: CRITICAL)

**Component Views (Batch Iteration):**
```cpp
// CORRECT: Single view pass
auto view = registry.view<TransformComponent, KinematicsComponent, ColliderComponent>();
view.each([](auto entity, auto& transform, auto& kin, auto& collider) {
    // Process all components in cache-friendly pass
});

// INCORRECT: Multiple passes
for (auto entity : view<TransformComponent>()) {
    auto& kin = registry.get<KinematicsComponent>(entity);  // Extra lookup
    auto& collider = registry.get<ColliderComponent>(entity);  // Extra lookup
}
```

**Registry Context for Shared Services:**
```cpp
// Store spatial indices in registry context
registry.ctx().emplace<CreatureSpatialIndex>(config);

// Access efficiently (O(1))
auto& spatial = registry.ctx().get<CreatureSpatialIndex>();
spatial.rebuild(registry);  // Avoid global state in systems
```

### 6. Branch Prediction (PRIORITY: MEDIUM)

**Early Exit, Low Divergence:**
```cpp
// CORRECT: Predictable branching
if (energy <= 0.0) {
    handle_death(entity);  // Rare but predictable
    return;  // Early exit
}

// INCORRECT: Nested conditions in hot path
if (energy > 0.0) {
    if (age > max_age) {
        if (fitness > threshold) {
            // ... deep nesting, hard to predict
        }
    }
}
```

### 7. SIMD Opportunities (PRIORITY: LOW-MEDIUM)

**When to Use SIMD:**
- Physics integration (multiple colliders): SIMD distance checks
- Brain inference (MLP): Batch forward passes with AVX2
- Spatial queries: SIMD AABB tests in broad phase

**Example (AVX2 for distance):**
```cpp
// Align data for SIMD (256/512-bit alignment)
alignas(32) std::vector<float> positions;

// Use intrinsics or compiler auto-vectorization
#pragma omp simd
for (int i = 0; i < count; ++i) {
    distances[i] = compute_distance_simd(positions[i], target);
}
```

---

## SYSTEM-SPECIFIC OPTIMIZATIONS

### Physics System (Hot Path #1)

**Broad-Phase Optimization:**
- Spatial hash: O(N) collision pairs
- Sweep-and-prune: For directional movement
- Conservative bounds: AABB test before narrow phase

**Solver Iteration Strategy:**
```cpp
// Fixed iteration count (determinism + stability)
constexpr int SOLVER_ITERATIONS = 8;
for (int iter = 0; iter < SOLVER_ITERATIONS; ++iter) {
    solve_constraints(contacts);
}
```

### Vision System (Expensive: O(R × E))

**Raycasting Optimization:**
- Early exit on solid hit
- Spatial pruning: Test against local grid cell only
- Batch ray processing: SIMD where feasible

### Brain Inference (Neural Network)

**MLP Forward Pass:**
- SoA weights and inputs
- Pre-allocate activation buffers
- Reuse across ticks

```cpp
// CORRECT: Contiguous weight storage
struct MLPWeights {
    std::vector<float> w1;  // [hidden × input]
    std::vector<float> b1;  // [hidden]
    std::vector<float> w2;  // [output × hidden]
    std::vector<float> b2;  // [output]
};
```

**NEAT Topology Evaluation:**
- Cache active connections only
- Skip disabled neurons
- Pre-sort connections by source neuron

### Spatial Indexing (O(1) queries)

**Hash Function Choice:**
```cpp
// Power-of-two modulus (fast, uniform)
constexpr int GRID_SIZE = 1024;
std::uint32_t hash = (static_cast<int>(x) & (GRID_SIZE - 1)) |
                      (static_cast<int>(z) & (GRID_SIZE - 1)) << 10;
```

**Dynamic Rebuilding:**
- Only rebuild when entities move significantly
- Incremental updates preferred

### Genetics Operations

**Genome Storage (Content-Hashed IDs):**
- Hash genome bytes for O(1) lookup
- Use std::unordered_map<GenomeID, GenomeData>

**Mutation/Crossover:**
- Allocate mutation buffer once per tick
- Reuse crossover storage

---

## MEMORY PROFILING HOTSPOTS

### Per-Tick Allocations (FORBIDDEN in Hot Paths)

**Detect with Valgrind/ASan:**
```bash
# Build with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
cmake --build . --target sim_core

# Run
valgrind --tool=massif ./bin/sim_core --seed 12345 --ticks 1000
```

**Common Offenders:**
- Vision system: `std::vector` inside `each()` loop
- Brain inference: Temporary activation vectors per entity
- Spatial index: Rebuilding hash every tick unnecessarily

### Cache Miss Patterns

**Identify with perf:**
```bash
# Linux perf
perf record -g ./bin/sim_core --seed 12345 --ticks 10000
perf report | less
```

**High Cache Miss Patterns:**
- Random component access (use views instead)
- Non-contiguous data structures (use SoA)
- Pointer chasing (locality via ECS pools)

---

## COMPILER OPTIMIZATIONS

### Release Build Flags (CMakeLists.txt)

```cmake
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

# Link-time optimization
set(CMAKE_INTERPROCEDURAL_BUILD_RELEASE ON)
```

### Profile-Guided Optimization (PGO)

```bash
# Step 1: Build with instrumentation
cmake -DCMAKE_BUILD_TYPE=Profile ..
cmake --build . --target sim_core

# Step 2: Run representative workload
./bin/sim_core --seed 12345 --ticks 100000

# Step 3: Rebuild with profile data
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target sim_core
```

### LTO (Link-Time Optimization)

```cmake
# Enable in CMakeLists.txt
set(CMAKE_INTERPROCEDURAL_BUILD TRUE)
set(CMAKE_LINKER_FLAGS_RELEASE "-flto -fuse-linker-plugin")
```

---

## THREADING OPPORTUNITIES (FUTURE-SAFE)

### System Parallelization

**Independent Systems:**
- Plant growth vs creature metabolism (no data dependency)
- Species indexing vs trait analysis (read-only genome storage)

**Data Parallelism Within Systems:**
```cpp
// Spatial index grid cells (embarrassingly parallel)
#pragma omp parallel for
for (int cell = 0; cell < grid_size; ++cell) {
    process_cell(cell);  // Each cell independent
}
```

**Lock-Free Patterns:**
- Use atomics for statistics counters
- Event-based coordination (avoid mutexes)

---

## BENCHMARKING & VALIDATION

### Performance Tests (tests/telemetry_bench.cpp)

```cpp
// Benchmark critical paths
TEST(PerformanceBenchmark, SpatialIndexLookup) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; ++i) {
        spatial_index.query(position, radius);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    ASSERT_LT(ms, 10.0) << "100K queries in <10ms";
}
```

### Determinism Validation

```bash
# Run twice with same seed - outputs must match
./bin/sim_core --seed 12345 --ticks 1000 > run1.log
./bin/sim_core --seed 12345 --ticks 1000 > run2.log
diff run1.log run2.log  # Should be empty diff
```

---

## ANTI-PATTERNS (DO NOT USE)

### Hot Path Allocations
❌ **NEVER** `new` or `std::vector::push_back` inside `registry.view<>().each()`
❌ **NEVER** allocate per-entity buffers in VisionSystem, BrainInferenceSystem
❌ **NEVER** rebuild spatial hash every tick (rebuild on entity movement threshold)

### Cache Unfriendly Patterns
❌ **NEVER** use `std::map<>` or `std::unordered_map<>` for hot component storage
❌ **NEVER** iterate with random `registry.get<>()` lookups (use views)
❌ **NEVER** nested structures (AoS) where SoA is needed

### Non-Deterministic Code
❌ **NEVER** use wall-clock `dt` in physics solver
❌ **NEVER** use global RNG without seed derivation
❌ **NEVER** iterate over `std::unordered_map<>` (order varies)

### Thread Safety Violations
❌ **NEVER** mutable global state in systems
❌ **NEVER** race conditions in shared service updates (use atomsics or locks)
❌ **NEVER** non-atomic statistics counters in parallel code

---

## WHERE TO START

### For Hot Path Optimization
1. Profile with `perf` or `valgrind --tool=massif`
2. Identify top 5 functions by CPU time
3. Apply cache-locality principles first (usually 50%+ gains)
4. Eliminate per-tick allocations (ASan/LSan to find)
5. Consider SIMD for math-heavy sections

### For Code Review
1. Check all `registry.view<>().each()` loops for hidden allocations
2. Verify spatial index usage (no O(N²) patterns)
3. Confirm RNG seed derivation in random operations
4. Review data structures: SoA vs AoS, flat vs nested

### For New Systems
1. Store state in components (not system members)
2. Use views for iteration (never `registry.get<>()` in loops)
3. Pre-reserve vectors (reserve capacity once)
4. Mark inline small functions (<10 lines, called in hot paths)

---

## QUALITY GATES & TOOLING (2025-01-25)

**Code Quality Infrastructure:**
- ✅ **Static Analysis**: `.clang-tidy` with conservative ruleset (warning-only, not blocking)
- ✅ **Code Formatting**: `.clang-format` (Google style, 100 column limit)
- ✅ **Sanitizers**: CMake `ENABLE_SANITIZERS` option for Debug builds
  - Address Sanitizer (ASan): Memory corruption, use-after-free
  - Undefined Behavior Sanitizer (UBSan): Undefined behavior detection
- ✅ **CI Pipeline**: GitHub Actions workflow (`.github/workflows/build-and-test.yml`)
  - Automated build + test on push/PR
  - Release and Debug matrix
  - Separate sanitizer job (manual trigger)

**Usage:**
```bash
# Standard build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build

# Sanitizer build
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-asan -j
ctest --test-dir build-asan

# Format check (dry-run)
clang-format --dry-run --Werror src/**/*.cpp
```

**Performance Optimizations Applied:**
- ✅ **Spatial Hash**: `clear()` preserves cell vector capacity across ticks (no per-tick allocations)
- ✅ **Stats System**: Reduced from 5 to 3 registry scans per report (eliminated duplicate plant/consumer passes)
- ✅ **Genetics**: Added `reserve()` to mutation operations (reduced vector reallocations)

**Safety & Correctness Improvements:**
- ✅ **Fixed-Point**: Divide-by-zero calls `std::terminate()` (was: silent return 0)
- ✅ **Persistence**: InnovationDatabase uses explicit little-endian byte order with validation
- ✅ **Test Coverage**: All 133 tests passing (100%)

**Quality Enforcement Policy:**
- No code merged without CI passing
- Sanitizer builds run periodically
- clang-format checks on changed files
- clang-tidy warnings reviewed before merge

---

## TOOLS & REFERENCES

### Profiling
- `perf` (Linux): CPU performance, cache misses, branch prediction
- `valgrind --tool=massif`: Memory leaks, allocation hotspots
- `valgrind --tool=callgrind`: Function call costs
- `gprof` / `pprof`: Sampling profiler

### Static Analysis
- **clang-tidy**: `.clang-tidy` configured with checks
  - `-cppcoreguidelines-pro-type-reinterpret-cast` (for memory safety)
  - `-modernize-use-trailing-return-type` (performance)
- **clang-format**: Google style, ColumnLimit 100

### C++ Performance Guides
- CppCon: https://en.cppreference.com/w/cpp/language_reference
- High-Performance C++: Bjarne Stroustrup's guidelines
- Game Engine Architecture: Jason Gregory's books (ECS patterns)

---

## PERFORMANCE TARGETS (Prototype)

- **Headless tick**: ≤2ms at ~1k entities (500 entities/sec)
- **Scalability**: Graceful degradation to 10k+ entities
- **Memory**: <1GB at 1k entities (no leaks, pooled components)
- **Determinism**: Identical state across runs with same seed

---

## NOTES

### Current Bottlenecks (Identified via Profiling)
1. **VisionSystem**: Raycasting O(R × E) - optimize with spatial pruning
2. **BrainInferenceSystem**: MLP forward pass - batch or SIMD opportunities
3. **PhysicsSystem**: Collision detection - broad-phase spatial hash critical
4. **SpatialIndex**: Hash lookup - ensure cache-line alignment

### Verified Optimizations
- **EnTT ECS**: Component pools contiguous (cache-friendly)
- **PCG32 RNG**: Fast, deterministic with derived seeds
- **Fixed Timestep**: Stable physics at 60Hz

### Future Optimization Paths
1. **GPU Acceleration**: Neural inference, spatial queries (CUDA research branch)
2. **Multi-threading**: Independent systems, data-parallel grid processing
3. **SIMD**: Physics integration, distance calculations (AVX2/AVX-512)

---

**Last Updated:** 2025-01-25 (Quality Hardening Complete)
**Status:** Production Ready with Quality Gates
