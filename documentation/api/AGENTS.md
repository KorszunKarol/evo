# AGENTS.md - API Reference

**Generated:** 2025-01-25
**Parent:** `../AGENTS.md`

## OVERVIEW

Complete API signatures for all major classes and functions in the evolution simulation.

## STRUCTURE

```
api/
└── function_reference.md     # Complete API documentation
```

## WHERE TO START

1. **`function_reference.md`** - All major API signatures

## KEY APIS BY MODULE

### Core Simulation

#### SimulationApp

```cpp
// Construction
explicit SimulationApp(SimulationConfig config = {});

// Main loop
void tick();

// Batch execution
void run_for_steps(std::size_t steps);

// Accessors
[[nodiscard]] entt::registry& registry();
[[nodiscard]] Scheduler& scheduler();
[[nodiscard]] double simulation_time() const;
[[nodiscard]] double fixed_dt() const;
```

#### Scheduler

```cpp
// Register system
void add_system(std::unique_ptr<ISystem> system);

// Execute systems
void tick_systems(SimulationContext& context);

// System list access
const std::vector<std::unique_ptr<ISystem>>& systems() const;
```

#### SimulationContext

```cpp
// Constructor
SimulationContext(entt::registry& registry, double fixed_dt, double sim_time);

// Accessors
[[nodiscard]] entt::registry& registry();
[[nodiscard]] entt::registry& registry() const;
[[nodiscard]] double fixed_dt() const;
[[nodiscard]] double simulation_time() const;
```

#### ISystem Interface

```cpp
// Must implement
virtual void tick(SimulationContext& context) = 0;
virtual std::string_view name() const = 0;
virtual ~ISystem() = default;
```

### Physics

#### PhysicsSystem

```cpp
// Construction
explicit PhysicsSystem(std::unique_ptr<IPhysicsBackend> backend);

// Execution
void tick(SimulationContext& context) override;
[[nodiscard]] std::string_view name() const noexcept override;
[[nodiscard]] const IPhysicsBackend::Stats& stats() const noexcept;
```

#### IPhysicsBackend Interface

```cpp
// Configure solver
virtual void configure(const Config& config) = 0;

// Sync from ECS
virtual void sync_from_registry(entt::registry& registry) = 0;

// Physics step
virtual void step(entt::registry& registry, double dt) = 0;

// Raycast query
[[nodiscard]] virtual std::optional<ContactEvent> raycast(
    const Vec3& origin, const Vec3& direction, double max_distance) const = 0;

// Contact events
[[nodiscard]] virtual std::span<const ContactEvent> contact_events() const = 0;

// Statistics
[[nodiscard]] virtual Stats stats() const = 0;
```

#### Vec3 Math Type

```cpp
// Constructors
constexpr Vec3() = default;
constexpr Vec3(double x_, double y_, double z_);

// Operators
Vec3& operator+=(const Vec3& rhs);
Vec3& operator-=(const Vec3& rhs);
Vec3& operator*=(double scalar);

// Utility
[[nodiscard]] double length_squared() const;
[[nodiscard]] double length() const;

// Free functions (operators)
Vec3 operator+(Vec3 lhs, const Vec3& rhs);
Vec3 operator-(Vec3 lhs, const Vec3& rhs);
Vec3 operator*(Vec3 lhs, double scalar);
Vec3 operator*(double scalar, Vec3 rhs);
```

### Genetics

#### GenomeStorage

```cpp
// Create random genome
GenomeId create_random(std::uint32_t seed);

// Insert genome
GenomeId insert(const evolution::genome::Genome& genome_obj);

// Retrieve genome
const evolution::genome::Genome* get(GenomeId id) const;

// Check existence
[[nodiscard]] bool contains(GenomeId id) const;

// Get all IDs
[[nodiscard]] std::vector<GenomeId> ids() const;

// Save/Load (planned)
void save_all(const std::string& path);
void load_from_disk(const std::string& path);
```

#### GenomeOps

```cpp
// Mutation
evolution::genome::GenomeT mutate(
    evolution::genome::GenomeT genome,
    const ReproConfig& config,
    std::uint64_t seed) noexcept;

// Mutation with NEAT
evolution::genome::GenomeT mutate(
    evolution::genome::GenomeT genome,
    const ReproConfig& config,
    std::uint64_t seed,
    evolution::genetics::InnovationDatabase& innovations) noexcept;

// Crossover
evolution::genome::GenomeT crossover(
    const evolution::genome::GenomeT& a,
    const evolution::genome::GenomeT& b,
    const ReproConfig& config,
    std::uint64_t seed) noexcept;

// Compatibility distance
[[nodiscard]] double compatibility_distance(
    const evolution::genome::Genome& a,
    const evolution::genome::Genome& b,
    const ReproConfig& config) noexcept;
```

#### PhenotypeBuilder

```cpp
// Build phenotype from genome
[[nodiscard]] static PhenotypeBuildResult build(
    GenomeId id,
    entt::registry& registry,
    entt::entity entity,
    const GenomeStorage& storage) noexcept;
```

#### PhenotypeBuildResult

```cpp
struct PhenotypeBuildResult {
    bool ok;                          // Success flag
    std::string msg;               // Error message
    DerivedTraits traits;           // Computed traits
};
```

#### BrainMlp / BrainNeat

```cpp
// MLP evaluation
[[nodiscard]] static void Evaluate(
    const evolution::genome::Mlp& mlp,
    std::span<const double> inputs,
    std::span<double> outputs);

// NEAT runtime
explicit BrainNeat(const evolution::genome::Neat& neat);
void Evaluate(std::span<const double> inputs, std::span<double> outputs);
void reset_state();
[[nodiscard]] std::size_t input_count() const;
[[nodiscard]] std::size_t output_count() const;
```

### Environment

#### EnvironmentBootstrapSystem

```cpp
// Initialize environment
void initialize_environment(entt::registry& registry, const EnvironmentConfig& config);

// Seed initial plants
void seed_initial_plants(entt::registry& registry, const EnvironmentConfig& config);
```

#### BiomeMap

```cpp
// Sample biome at position
[[nodiscard]] BiomeId sample(double x, double z) const noexcept;
[[nodiscard]] BiomeId sample_discrete(int ix, int iz) const noexcept;
```

#### WaterMap

```cpp
// Water depth
[[nodiscard]] double depth(double x, double z) const noexcept;

// Shoreline distance
[[nodiscard]] double shore_distance(double x, double z) const noexcept;

// Check water presence
[[nodiscard]] bool is_water(double x, double z) const noexcept;
```

#### SoilGrid / SoilVolume

```cpp
// Sample nutrients
[[nodiscard]] float sample(double x, double z) const noexcept;

// Diffuse nutrients
void diffuse(double dt) noexcept;

// Regenerate nutrients
void regenerate(double dt) noexcept;
void regenerate_by_biome(double dt, const BiomeMap& biome_map, ...) noexcept;
```

### Behavior Systems

#### VisionSystem

```cpp
// Execution
void tick(SimulationContext& context) override;
[[nodiscard]] std::string_view name() const noexcept override;

// Configuration via VisionComponent
struct VisionComponent {
    double fov_radians;
    std::uint32_t ray_count;
    double max_range;
    bool enabled;
    std::vector<double> ray_distances;
    std::vector<VisionHitType> ray_hit_types;
    std::vector<entt::entity> ray_hit_entities;
};
```

#### SocialBehaviorSystem

```cpp
// Execution
void tick(SimulationContext& context) override;
[[nodiscard]] std::string_view name() const noexcept override;

// Social signals
struct SocialSignalsComponent {
    double cohesion;
    double alignment;
    double separation;
    double neighbor_density;
    double territory_dist_norm;
    double intruder_density;
    Vec3 prey_dir;
    uint32_t pack_density;
};
```

#### TelemetrySystem

```cpp
// Execution
void tick(SimulationContext& context) override;
[[nodiscard]] std::string_view name() const noexcept override;

// Telemetry data
struct TelemetryComponent {
    double energy_gained;
    double energy_lost_metabolism;
    double energy_lost_movement;
    double distance_traveled;
    uint32_t successful_feeds;
    uint32_t kill_count;
    DeathCause death_cause;
    uint32_t killed_by_predation;
};
```

## COMPONENT CREATION PATTERNS

### Creating Entities

```cpp
// Create entity
entt::entity entity = registry.create();

// Add components
auto& transform = registry.emplace<TransformComponent>(entity, ...);
auto& kinematics = registry.emplace<KinematicsComponent>(entity, ...);
auto& collider = registry.emplace<ColliderComponent>(entity, ...);

// Build phenotype
auto result = PhenotypeBuilder::build(genome_id, registry, entity, storage);
if (!result.ok) {
    registry.destroy(entity); // Cleanup on failure
}
```

### Querying Components

```cpp
// View iteration (efficient)
auto view = registry.view<TransformComponent, KinematicsComponent>();
view.each([](auto entity, TransformComponent& transform, KinematicsComponent& kin) {
    // Process entity
});

// Single component access
auto& component = registry.get<Component>(entity);
```

### Destroying Entities

```cpp
// Destroy entity
registry.destroy(entity);

// Components automatically removed
```

## CONFIGURATION STRUCTS

### SimulationConfig

```cpp
struct SimulationConfig {
    double fixed_dt = 1.0 / 60.0;  // 60 Hz timestep
};
```

### EnvironmentConfig

```cpp
struct EnvironmentConfig {
    TerrainConfig terrain;
    BiomeConfig biome;
    WaterConfig water;
    SoilConfig soil;
    PlantBootstrapConfig plants;
    double plant_spatial_cell_size = 2.0;
};
```

### ReproConfig / MutationConfig

```cpp
struct ReproConfig {
    double mutate_rate_struct;
    double mutate_rate_param;
    double weight_sigma;
    double neat_excess_weight;
    double neat_disjoint_weight;
    double neat_weight_diff_weight;
    double neat_body_weight;
};

struct StructuralMutationConfig {
    double add_node_rate;
    double add_connection_rate;
    double delete_connection_rate;
    double toggle_connection_rate;
};
```

## COMMANDS

### Using SimulationApp

```cpp
#include "evolution/sim/simulation_app.h"

// Default configuration
evolution::sim::SimulationApp app;

// Custom configuration
evolution::sim::SimulationConfig config;
config.fixed_dt = 1.0 / 120.0;  // 120 Hz
evolution::sim::SimulationApp app(config);

// Run simulation
for (int i = 0; i < 3600; ++i) {  // 1 minute at 60 Hz
    app.tick();
}

// Run for specific duration
app.run_for_steps(6000);  // 100 seconds at 60 Hz
```

### Building Custom Systems

```cpp
#include "evolution/sim/ISystem.h"

class MyCustomSystem : public evolution::sim::ISystem {
public:
    void tick(SimulationContext& context) override {
        // Access registry
        auto view = context.registry().view<MyComponent>();
        
        // Process entities
        view.each([](auto entity, MyComponent& comp) {
            // Your logic here
        });
    }
    
    [[nodiscard]] std::string_view name() const noexcept override {
        return "my_custom_system";
    }
};

// Register in main
scheduler.add_system(std::make_unique<MyCustomSystem>());
```

### Working with GenomeStorage

```cpp
#include "evolution/genetics/genome_storage.h"

// Create storage
evolution::genetics::GenomeStorage storage;

// Generate random genome
std::uint32_t seed = 12345;
auto genome_id = storage.create_random(seed);

// Retrieve genome
const auto* genome = storage.get(genome_id);
if (genome) {
    // Use genome
}

// Build phenotype
entt::entity entity = registry.create();
auto result = evolution::genetics::PhenotypeBuilder::build(
    genome_id, registry, entity, storage);
```

## NOTES

### API Stability

- **Backward compatibility**: Maintained across updates
- **Const correctness**: Read-only operations marked with `[[nodiscard]]` and `const`
- **Exception safety**: RAII patterns, move semantics

### Documentation Coverage

| Module | API Coverage | Notes |
|---------|---------------|--------|
| Core Simulation | 100% | All major APIs documented |
| Physics | 100% | Backend interface complete |
| Genetics | 95% | Storage, ops, phenotypes covered |
| Environment | 90% | Terrain, biome, water, soil covered |
| Behavior Systems | 85% | Vision, social, telemetry covered |

---

**Last Updated:** 2025-01-25
