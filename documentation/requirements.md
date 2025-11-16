## MVP Environment Plan (SSOT)

### Purpose
- Serve as the single source of truth for scope, acceptance, performance, and determinism requirements.
- Any scope change must be reflected here.

### MVP Environment (v1.0)
- **Objectives:**
  - Biomes: deterministic biome map (2–4 types) that drives soil regen and flora mix.
  - Water: static lakes/rivers derived from terrain; depth and shoreline affect plant species.
  - Flora: 3–5 plant species with distinct traits (growth, energy, seeding, toxicity optional).
  - Climate cadence: day/night multiplier; optional seasonal multiplier.
  - Viewer: skybox, textured terrain, water rendering, instanced flora, biome overlays.
- **Out of scope now:**
  - Dynamic climate drift, erosion simulation, buoyancy/fluids, plant coevolution defenses, fauna predators.

### Feature Requirements
- **Terrain v2**
  - Biome mask: multi-octave noise producing 2–4 labeled biomes; deterministic per seed.
  - Water mask: lakes from height threshold (water_level), rivers from flow accumulation skeleton (lightweight); mark water depth and shoreline.
  - Acceptance:
    - The same seed yields the same biome and water masks.
    - At least 20% land area above water.
- **Soil/Climate**
  - Soil regen per biome (regen_rate, baseline); day/night multiplier ∈ [0.5, 1.2]; optional seasonal factor ∈ [0.8, 1.2] on a long period.
  - Acceptance:
    - Mean soil converges to biome-specific baselines (±ε).
- **Flora Species**
  - Species definition: growth_rate, max_energy, seed_radius, seed_interval, establish_prob, radius.
  - Spawn species by biome and water proximity bucket (aquatic, shoreline, terrestrial).
  - Acceptance:
    - Species distributions reflect biome allocations (±10% tolerance).
- **Water Integration**
  - Plants: aquatic species only spawn where depth ≥ threshold; shoreline species within [d_min, d_max]; terrestrial otherwise.
  - Physics: no buoyancy in MVP; ground collisions unchanged.
- **Viewer**
  - Terrain: textured heightfield; optional biome debug overlay.
  - Water: simple planar or heightfield masking with Fresnel-like shader; depth-based color blend.
  - Plants: instanced rendering; color by species; toggle bounding/energy overlay.
  - HUD: per-biome biomass, soil mean, plant counts.
- **Telemetry**
  - Time series: total plant biomass, per-biome biomass, soil means, plant counts by species.
  - Snapshots: optional periodic snapshot of environment state (terrain checksum, soil mean).

### Contracts (Unchanged Surfaces)
- **Services:**
  - `Terrain`, `SoilGrid`, `PlantSpatialIndex`, `FeedingStatistics` in `registry.ctx<T>()`.
  - Add `BiomeMap` and `WaterMap` services for environment queries.
- **Components:**
  - `PlantComponent`, `PlantSeedParams` remain; extend only if strictly necessary (e.g., `species_id`).
- **Systems ordering (per tick):**
  - `SoilSystem` → `PlantGrowthSystem` → `PlantSeedingSystem` → `PlantSpatialSystem` → `FeedingSystem` → `PlantCleanupSystem` → others.
- **Determinism:**
  - All environment generation and stochastic seeding use deterministic RNG (seeded from global seed + op tags).

### Performance Targets (headless)
- 10–30k plants, 1–3k herbivores @ 60 Hz (single thread).
- Soil diffusion sub-sampled; instancing for plants; viewer @ 1080p60 with 10k+ instances.

### Acceptance Criteria (MVP)
- Biomes: deterministic 2–4 biome mapping, controls soil regen and flora mix.
- Water: deterministic lakes/rivers; aquatic/shoreline/terrestrial species distributions reflect water depth/proximity.
- Flora: 3–5 species with distinct parameters; biomass and counts trend to steady-state with default configs.
- Viewer: skybox, terrain texture, water rendering, plant instancing; biome overlay toggle.
- Telemetry: per-biome biomass and soil means log periodically; snapshot optional.
- Perf: within targets; determinism validated by seed.

### Phased Implementation
- **Phase 1 (3–4 days)**
  - BiomeMap service (noise-based), WaterMap (threshold lakes + simple flow skeleton).
  - Terrain viewer texture; water plane rendering; biome overlay in viewer.
  - Tests: mask determinism, allocation ratios, perf microbenchmarks.
- **Phase 2 (3–4 days)**
  - Soil regen by biome + day/night; species definitions; spawn across biomes and water zones; plant instancing by species.
  - Stats: per-biome biomass and soil means; HUD display.
- **Phase 3 (2–3 days)**
  - Stability tuning (seed intervals, establish prob); shoreline logic; telemetry snapshot; viewer polishing (depth-based water color).
- **Optional Stretch (2–3 days)**
  - Seasonal multiplier; toxicity trait + herbivore penalty; water flow map debug.

### Testing Strategy
- **Unit**: biome mask, water mask determinism; soil regen curves; seeding acceptance checks.
- **Integration**: biomass trends under default params; species prevalence by biome; shoreline species within target bands.
- **Performance**: tick time budgets across plant counts; viewer instance draw throughput.
- **Determinism**: snapshot runs revalidate masks and distributions across seeds.


