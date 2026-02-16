# Documentation Index

## Single Source of Truth

- [MVP Environment Plan (SSOT)](./requirements.md) — environment MVP scope, acceptance, contracts, and phased plan.

## Architecture Documentation

- [Architecture Overview](./architecture/overview.md) - High-level system design
- [Recovery Baseline (2026-02-16)](./architecture/recovery_baseline.md) - Stabilization checkpoint and resolved failures
- [Module Ownership](./architecture/module_ownership.md) - Ownership boundaries and review escalation
- [Branching Playbook](./architecture/branching_playbook.md) - Lightweight protected-branch workflow without CI gates
- [Module Documentation](./modules/) - Detailed module specifications
- [API Reference](./api/) - Complete function signatures
- [Data Contracts](./data-contracts/) - Inter-module communication

## Module Documentation

- [Core Simulation Module](./modules/core_simulation.md) - SimulationApp, Scheduler, Context
- [Physics System Module](./modules/physics_system.md) - PhysicsSystem, IPhysicsBackend, SimplePhysicsBackend
- [Render Client Module](./modules/render_client.md) - OpenGL visualization client
- [Components Module](./modules/components.md) - ECS component definitions
- [Genome Module](./modules/genome.md) - Genome storage, operations, and RNG utilities
- [Phenotype Module](./modules/phenotype.md) - Building ECS entities from genomes
- [Brain Module](./modules/brain.md) - MLP and NEAT inference engines
- [Telemetry Module](./modules/telemetry.md) - Event and rollup telemetry pipeline
- [Reproduction Module](./modules/reproduction.md) - Mating, crossover, mutation system
- [Species Index Module](./modules/species_index.md) - Species clustering system
- [Scenario Module](./modules/scenario.md) - Simulation configuration and initialization

## API Reference

- [Function Reference](./api/function_reference.md) - Complete API documentation

## Data Contracts

- [Inter-Module Contracts](./data-contracts/inter_module_contracts.md) - Data flow and guarantees
- [Genetics Contracts](./data-contracts/genetics.md) - Genome, phenotype, and brain data flows

## Quick References

### Finding Documentation

**By Component**:
- TransformComponent → [Components Module](./modules/components.md#transformcomponent)
- KinematicsComponent → [Components Module](./modules/components.md#kinematicscomponent)
- MetabolismComponent → [Components Module](./modules/components.md#metabolismcomponent)

**By Class**:
- SimulationApp → [Core Simulation Module](./modules/core_simulation.md#simulationapp)
- Scheduler → [Core Simulation Module](./modules/core_simulation.md#scheduler)
- PhysicsSystem → [Physics System Module](./modules/physics_system.md#physicssystem)
- GenomeStorage → [Genome Module](./modules/genome.md#genomestorage)
- PhenotypeBuilder → [Phenotype Module](./modules/phenotype.md#phenotypebuilder)
- BrainInferenceSystem → [Core Simulation Module](./modules/core_simulation.md#braininferencesystem)
- MotorSystem → [Core Simulation Module](./modules/core_simulation.md#motorsystem)
- ReproductionSystem → [Reproduction Module](./modules/reproduction.md#reproductionsystem)
- SpeciesIndexSystem → [Species Index Module](./modules/species_index.md#speciesindexsystem)
- SimulationScenario → [Scenario Module](./modules/scenario.md#simulationscenario)

**By Function**:
- `SimulationApp::tick()` → [API Reference](./api/function_reference.md#tick)
- `Scheduler::add_system()` → [API Reference](./api/function_reference.md#add_system)
- `PhysicsSystem::tick()` → [API Reference](./api/function_reference.md#tick-1)
- `setup_scenario()` → [Scenario Module](./modules/scenario.md#setup_scenario)
- `seed_initial_population()` → [Scenario Module](./modules/scenario.md#seed_initial_population)

**By Data Flow**:
- Component access → [Data Contracts](./data-contracts/inter_module_contracts.md#component-access-contracts)
- System execution → [Data Contracts](./data-contracts/inter_module_contracts.md#system-execution-contracts)
- Force accumulation → [Data Contracts](./data-contracts/inter_module_contracts.md#force-accumulation-flow)
- Genome storage → [Genetics Contracts](./data-contracts/genetics.md#genome-storage-contracts)
- Phenotype building → [Genetics Contracts](./data-contracts/genetics.md#phenotype-building-contracts)
- Brain inference → [Genetics Contracts](./data-contracts/genetics.md#brain-inference-contracts)
- Reproduction → [Genetics Contracts](./data-contracts/genetics.md#reproduction-contracts)
- Species indexing → [Genetics Contracts](./data-contracts/genetics.md#species-indexing-contracts)

## Documentation Standards

### Module Documentation Includes

- **Overview**: Purpose and responsibilities
- **Files**: Source files in module
- **Classes**: Complete class documentation
  - Purpose and responsibilities
  - Public API with parameters and returns
  - State management
  - Thread safety
  - Performance characteristics
- **Data Contracts**: How module interacts with others
- **Dependencies**: Internal and external dependencies
- **Extension Points**: How to extend the module

### API Documentation Includes

- **Function Signature**: Complete C++ signature
- **Parameters**: Type, purpose, preconditions, ranges
- **Returns**: Type, meaning, postconditions
- **Exceptions**: What exceptions may be thrown
- **Complexity**: Time and space complexity
- **Thread Safety**: Thread safety guarantees
- **Side Effects**: What the function modifies

### Data Contract Documentation Includes

- **Access Patterns**: Who reads/writes what
- **Data Formats**: Structure definitions
- **Guarantees**: What is guaranteed
- **Flow Diagrams**: Visual representation
- **Violations**: What causes undefined behavior

## Contributing Documentation

When adding new code:

1. **Update Module Documentation**: Add class to appropriate module doc
2. **Update API Reference**: Add function signatures
3. **Update Data Contracts**: Document new data flows
4. **Update Index**: Add links to new documentation

## Documentation Maintenance

- **Keep in sync**: Documentation should match code
- **Update on changes**: Modify docs when APIs change
- **Review regularly**: Ensure accuracy and completeness

## Runtime Contracts

- Runtime interfaces and health/run metadata types live in `sim/include/evolution/sim/runtime_contracts.h`.
- Environment interface adapters live in `sim/include/evolution/sim/environment/service_adapters.h`.

## Tooling References

- [Project Health Template](./tools/project_health.md) - Standardized health checkpoint format
