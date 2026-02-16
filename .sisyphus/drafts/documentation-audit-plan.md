# Documentation Audit & Improvement Plan

## Executive Summary

Documentation coverage is **good but incomplete**. Core systems and data contracts are well-documented, but several newer systems lack dedicated module docs. Architecture overview and data contracts are solid, but could benefit from additional diagrams.

---

## Coverage Analysis

### ✅ Well-Documented Areas

| Module | Coverage | Quality | Notes |
|---------|----------|---------|--------|
| Core Simulation | 100% | Excellent | SimulationApp, Scheduler, Context fully documented |
| Components | 95% | Excellent | All major components with data contracts |
| Physics System | 100% | Excellent | Backend, broad/narrow phase documented |
| Environment | 100% | Excellent | Terrain, soil, plants, feeding documented |
| Telemetry | 100% | Excellent | Event schema, rollup contracts documented |
| Genome | 100% | Excellent | Storage, mutation, phenotype documented |
| Brain (MLP/NEAT) | 100% | Excellent | Inference engines documented |
| Data Contracts | 100% | Excellent | Inter-module flows with ASCII diagrams |

### ❌ Missing Documentation

| Module | Status | Impact |
|--------|--------|--------|
| **ReproductionSystem** | ❌ No module doc | Critical: reproduction is core evolution mechanic |
| **SpeciesIndexSystem** | ❌ No module doc | High: species tracking affects selection |
| **Scenario** | ❌ No module doc | Medium: scenario setup is entry point |

---

## Specific Gaps

### 1. Undocumented Systems

#### ReproductionSystem (`sim/include/evolution/sim/reproduction_system.h`)
**Missing:**
- Module overview (purpose, responsibilities)
- Public API documentation
- Data contracts with GenomeStorage
- Species index integration contracts
- Genome inheritance logic documentation

**Impact:** HIGH - reproduction is fundamental to evolution simulation

#### SpeciesIndexSystem (`sim/include/evolution/sim/species_index_system.h`)
**Missing:**
- Module overview
- Clustering algorithm details (thresholds, distance metrics)
- Integration with reproduction system
- Data contracts for species querying

**Impact:** HIGH - species tracking used for fitness and selection

#### Scenario (`sim/include/evolution/sim/scenario.h`)
**Missing:**
- Module documentation
- Configuration contract
- Setup initialization workflow
- Integration with environment bootstrap

**Impact:** MEDIUM - scenario is the primary simulation entry point

---

### 2. Missing Data Contracts

| Contract Area | Missing | Priority |
|---------------|---------|----------|
| Reproduction → GenomeStorage | Genome lookup, inheritance patterns | HIGH |
| Reproduction → SpeciesIndex | Species assignment, threshold updates | HIGH |
| Scenario → EnvironmentBootstrap | Terrain/soil initialization contracts | MEDIUM |
| SpeciesIndex → Telemetry | Species event emission contracts | MEDIUM |
| Scenario → SpeciesIndex | Initial species registration | MEDIUM |

---

### 3. Missing Diagrams

| Diagram Type | Status | Recommended Tool |
|--------------|--------|-----------------|
| System execution order | ❌ ASCII only | Mermaid flowchart |
| Component lifecycle | ❌ Text description | Mermaid state diagram |
| Energy flow | ❌ ASCII in data contracts | Mermaid sequence diagram |
| Genome data flow | ❌ Text description | Mermaid sequence diagram |
| Environment tick pipeline | ❌ Text description | Mermaid flowchart |

**Recommendation:** Add Mermaid diagrams to documentation for:
1. Tick execution flow (already has ASCII, upgrade to Mermaid)
2. Energy flow between components (Metabolism → Feeding → Plants)
3. Genome inheritance pipeline (parent → child genomes)
4. Reproduction lifecycle (selection → crossover → mutation → spawn)

---

### 4. API Reference Gaps

**Analysis of `documentation/api/function_reference.md`:**
- ❓ Not verified (file exists, content unknown)
- Likely missing: newer system APIs (ReproductionSystem, SpeciesIndexSystem)

**Action Required:**
1. Cross-check function_reference.md against actual headers
2. Add missing public APIs:
   - `ReproductionSystem` methods
   - `SpeciesIndexSystem` methods
   - `SimulationScenario` configuration

---

### 5. Documentation Accuracy

**Verified Areas:**
- ✅ Architecture overview matches current code structure
- ✅ Environment systems documented (Terrain, Soil, Plants, Feeding)
- ✅ Data contracts in inter_module_contracts.md are accurate
- ✅ Component structure in components.md is up-to-date

**Outdated/Suspicious Areas:**
- ⚠️ Physics v1 vs v2? (`physics_v1.md` exists - is v2 documented?)
- ⚠️ Render client status (listed but implementation may be minimal)
- ⚠️ Future GPU/Rendering plans in architecture.md may be outdated

---

## Improvement Plan

### Phase 1: Complete Missing Module Docs (HIGH PRIORITY)

**Tasks:**
1. Create `documentation/modules/reproduction.md`
   - Overview: reproduction mechanics
   - Classes: `ReproductionSystem`
   - API: `tick()`, `get_parents()`, `select_mate()`
   - Data contracts: GenomeStorage integration
   - Dependencies: SpeciesIndex, FitnessComponent

2. Create `documentation/modules/species_index.md`
   - Overview: species clustering algorithm
   - Classes: `SpeciesIndexSystem`
   - API: `tick()`, `get_species_id()`, `get_population()`
   - Data contracts: threshold management, species querying
   - Algorithm details: distance metric, clustering

3. Create `documentation/modules/scenario.md`
   - Overview: scenario configuration and setup
   - Classes: `SimulationScenario`, `ScenarioBuilder`
   - API: `setup_scenario()`, configuration fields
   - Data contracts: environment bootstrap integration

**Estimated Effort:** 4-6 hours

---

### Phase 2: Add Data Contracts (HIGH PRIORITY)

**Tasks:**
1. Extend `documentation/data-contracts/genetics.md`
   - Add reproduction data flow
   - Genome inheritance contracts
   - Crossover/mutation guarantees

2. Extend `documentation/data-contracts/inter_module_contracts.md`
   - Add SpeciesIndexSystem contracts
   - Reproduction system contracts
   - Scenario setup contracts

**Estimated Effort:** 2-3 hours

---

### Phase 3: Add Mermaid Diagrams (MEDIUM PRIORITY)

**Tasks:**
1. Add Tick Execution Flow diagram (Mermaid)
   ```mermaid
   flowchart TD
       A[SimulationApp::tick] --> B[Create SimulationContext]
       B --> C[Scheduler::tick_systems]
       C --> D[MetabolismSystem]
       C --> E[PhysicsSystem]
       C --> F[BrainInferenceSystem]
       C --> G[MotorSystem]
   ```

2. Add Energy Flow Sequence diagram
   ```mermaid
   sequenceDiagram
       participant MS as MetabolismSystem
       participant FS as FeedingSystem
       participant P as PlantComponent
       participant M as MetabolismComponent
       MS->>M: Consume basal_rate * dt
       FS->>P: Read energy
       FS->>M: Add energy (clamped to max)
   ```

3. Add Reproduction Flow diagram
   ```mermaid
   flowchart TD
       A[Select Parents] --> B[Crossover]
       B --> C[Mutation]
       C --> D[Build Phenotype]
       D --> E[Spawn Entity]
       E --> F[Assign Species]
   ```

**Estimated Effort:** 2-3 hours

---

### Phase 4: Verify & Update API Reference (MEDIUM PRIORITY)

**Tasks:**
1. Audit `documentation/api/function_reference.md`
   - Cross-check against all system headers
   - Verify all public methods are documented
   - Add missing signatures for:
     - `ReproductionSystem`
     - `SpeciesIndexSystem`
     - `SimulationScenario`

2. Standardize API doc format
   - Ensure consistent parameter descriptions
   - Add complexity annotations
   - Add thread safety notes

**Estimated Effort:** 2-3 hours

---

### Phase 5: Architecture Visualizations (LOW PRIORITY)

**Tasks:**
1. Enhance `documentation/architecture/overview.md`
   - Add component structure diagram
   - Add system dependency graph
   - Add data flow between modules

2. Create `documentation/architecture/ecs_patterns.md`
   - Entity creation lifecycle
   - Component attachment/detachment
   - View iteration patterns

**Estimated Effort:** 3-4 hours

---

## Proposed File Additions

```
documentation/
├── modules/
│   ├── reproduction.md          [NEW] - Phase 1
│   ├── species_index.md          [NEW] - Phase 1
│   └── scenario.md              [NEW] - Phase 1
├── data-contracts/
│   ├── reproduction.md           [NEW] - Phase 2
│   └── species_index.md         [NEW] - Phase 2
└── architecture/
    ├── tick_flow.md             [NEW] - Phase 3
    ├── energy_flow.md           [NEW] - Phase 3
    ├── reproduction_flow.md      [NEW] - Phase 3
    └── ecs_patterns.md          [NEW] - Phase 5
```

---

## Documentation Quality Checklist

When adding new docs, ensure each includes:

- [ ] **Overview**: Purpose and responsibilities (1-2 paragraphs)
- [ ] **Files**: Source files in module
- [ ] **Classes**: Complete class documentation
  - [ ] Purpose and responsibilities
  - [ ] Public API with parameters/returns
  - [ ] State management
  - [ ] Thread safety
  - [ ] Performance characteristics
- [ ] **Data Contracts**: How module interacts with others
  - [ ] Read/write patterns
  - [ ] Guarantees provided
  - [ ] Preconditions/postconditions
- [ ] **Dependencies**: Internal and external dependencies
- [ ] **Extension Points**: How to extend the module
- [ ] **Diagrams**: At least one Mermaid diagram (if applicable)

---

## Priority Summary

| Priority | Phase | Tasks | Effort | Impact |
|----------|--------|-------|--------|
| **P0** | 1 | Add Reproduction, SpeciesIndex, Scenario module docs | 4-6 hrs | HIGH |
| **P0** | 2 | Add reproduction/species data contracts | 2-3 hrs | HIGH |
| **P1** | 4 | Verify and update API reference | 2-3 hrs | MEDIUM |
| **P1** | 3 | Add Mermaid flow diagrams | 2-3 hrs | MEDIUM |
| **P2** | 5 | Add architecture visualizations | 3-4 hrs | LOW |

**Total Estimated Effort:** 13-19 hours

---

## Success Metrics

Documentation will be considered "complete" when:

- ✅ All systems have module documentation (100% coverage)
- ✅ All inter-module data flows documented
- ✅ API reference matches all public headers
- ✅ At least 3 Mermaid diagrams in documentation/
- ✅ Documentation passes "quick reference" test:
  - Can I find `ReproductionSystem` API in < 10s?
  - Can I understand species clustering algorithm from docs?
  - Can I set up a custom scenario from docs?

---

## Next Steps

1. **Review this plan** with team for priority/accuracy
2. **Start with Phase 1** (missing module docs) - highest impact
3. **Iterative validation**: After each doc is added, verify it addresses the gaps identified
4. **Documentation audits**: Run this audit monthly to catch new gaps

---

## Recommended Tools

- **Diagrams**: Mermaid JS (supported by GitHub, VS Code)
- **API Extraction**: Use clang-doc or Doxygen to auto-generate signatures
- **Contract Validation**: Use compiler static_assert to validate invariants
- **Diagram Rendering**: Test Mermaid in GitHub preview before committing
