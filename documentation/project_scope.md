## Simplified Evolution Sim Plan

- Focus on core pillars: morphologically diverse creatures, neural-control evolution, ecological feedback; defer extras (parasites, climate drift, etc.) until sandbox works.
- **World**: Procedural heightmap + voxel/terrain chunking; start with 3 biomes (temperate forest, plains, lake); implement day/night cycle + ambient temp.
- **Sim loop**: Fixed/tick-based update with subsystems—physics, perception, brain inference, metabolism, reproduction, death, cleanup.
- **Physics/motion**: Articulated rigid bodies via PhysX/Bullet; constraint-based limbs with modular muscles; implicit IK for locomotion; implement pathfinding with simple impulse steering; use spatial hash for contacts.
- **Bodies**: Genome encodes tree of segments/limbs; each node has size/material parameters; generate mesh via metaballs or skinning; validate stability via feasible joint layout (`limbs<=6`, `length<range`).
- **Rendering**: Use Unity/Unreal; ECS prototype; instanced rendering for plants; LOD: sync/detailed for near creatures, simplified for far; creature shader handles color pattern genes.
- **Sensors**:
  - Vision: Raycasts per eye, sample color/size; limit to quadrant rays (8–12); feed distances into brain.
  - Touch: Contact events + surface type.
  - Internals: Energy, age, limb damage, reproduction timer.
- **Brains**: Use NEAT variant; modular genome describing nodes and weighted connections; incorporate gated recurrent units for memory; run inference at 5–10 ticks; GPU batching of forward passes.
- **Evolution**:
  - Reproduction: Sexual with crossovers; reproduction triggered by energy threshold.
  - Mutation: Body (segment scaling, add/remove limb within limits), brain (add node/edge, mutate weight), behavior.
  - Fitness = survival time + offspring + energy efficiency; kill degenerates.
  - Species cluster via compatibility distance.
- **Plants/resources**: Simple L-system plant genotype; growth rate, height, nutrient budget; random seeding; herbivores eat to gain energy; soil nutrient map.
- **Energy**: Track metabolic cost (mass, muscle usage, brain complexity). Movement/attack draws energy, rest recovers slowly. Decompose dead bodies into soil nutrients.
- **Predator–prey**: Basic behaviors from evolved brains; attack action reduces target HP; implement simple teeth/claw damage from genes.
- **UX**: Free camera, follow creature, inspect genome/body/brain (graph). Graph UI: population per species, mean traits. Save snapshots.

### Roadmap (≈6–9 months part-time)
- Month 1: Setup engine, terrain, ECS, tick loop.
- Month 2–3: Creature builder, genome -> morphology, physics rig, energy model.
- Month 3–4: Sensors, action interface; baseline neural controllers with simple heuristics.
- Month 4–5: Implement NEAT, selection, reproduction; run small populations.
- Month 5–6: Plants/resources, environmental feedback.
- Month 6–7: Visualization tools, data logging, stat graphs.
- Month 7–8: Optimizations (LOD, batching), GPU inference, cleaning.
- Month 8–9: Polishing, scenario presets, save/load.

### Stack Suggestion
- Engine: Unity DOTS or Unreal GAS (Unity easier for ECS + GPU).
- Physics: Stock engine physics + custom muscle constraints.
- ML libs: Custom NEAT, use compute shaders for inference.
- Data: Binary snapshots using protobuf or flatbuffer; telemetry via Python notebooks.

### Next steps
- Draft genome schema (body + brain).
- Prototype single creature with fixed brain to validate pipeline.
- Decide on ECS layout & dev skeleton.