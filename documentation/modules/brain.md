# Module: Brain Inference

## Overview

The brain module provides deterministic inference engines for MLP and NEAT neural networks. It supports feed-forward evaluation, recurrent state management (NEAT), and integration with the ECS actuation system.

## Files

- `sim/include/evolution/genetics/brain_mlp.h`
- `sim/src/genetics/brain_mlp.cpp`
- `sim/include/evolution/genetics/brain_neat.h`
- `sim/src/genetics/brain_neat.cpp`
- `sim/include/evolution/sim/brain_inference_system.h`
- `sim/src/brain_inference_system.cpp`

## Classes

### BrainMlp

**Purpose**: Feed-forward inference for multilayer perceptron genomes.

**Responsibilities**:
- Execute forward pass through MLP layers
- Apply activation functions (tanh for hidden, tanh for output)
- Handle variable input/output sizes (truncation/padding)

**Public API**:

```cpp
class BrainMlp {
public:
    static void Evaluate(const evolution::genome::MLP& mlp,
                         std::span<const double> inputs,
                         std::span<double> outputs) noexcept;
};
```

**Parameters**:

- `Evaluate(mlp, inputs, outputs)`:
  - **Input**:
    - `mlp`: Immutable FlatBuffers table describing network
    - `inputs`: Normalized sensor values (truncated/padded to `mlp.input_count()`)
    - `outputs`: Mutable span receiving network outputs (must match `mlp.output_count()`)
  - **Returns**: `void` (outputs written to span)
  - **Complexity**: O(S) where S = total weight count across layers
  - **Side Effects**: Writes to `outputs` span

**Network Structure**:

- **Input Layer**: `mlp.input_count()` neurons
- **Hidden Layers**: `mlp.hidden_layers()` array (e.g., `[16, 16]`)
- **Output Layer**: `mlp.output_count()` neurons
- **Weights**: Row-major packed array (`weights[i]` = weight for connection i)
- **Biases**: Packed array (`biases[i]` = bias for layer i)

**Activation Functions**:

- **Hidden Layers**: `tanh(x)` (bounded to `[-1, 1]`)
- **Output Layer**: `tanh(x)` (bounded to `[-1, 1]`)

**Weight Layout**:

For a network with `[input_count, hidden1, hidden2, output_count]`:

```
weights = [
    // Layer 1: input → hidden1
    w[0..input_count*hidden1-1],
    // Layer 2: hidden1 → hidden2
    w[input_count*hidden1..input_count*hidden1+hidden1*hidden2-1],
    // Layer 3: hidden2 → output
    w[...]
]
```

**Thread Safety**: Stateless; reentrant if inputs/outputs don't overlap.

### BrainNeat

**Purpose**: Inference engine for NEAT (Neuroevolution of Augmenting Topologies) networks.

**Responsibilities**:
- Topological sorting of enabled connections
- Feed-forward evaluation with recurrent edge support
- Runtime state caching for recurrent networks

**Public API**:

```cpp
class BrainNeat {
public:
    explicit BrainNeat(const evolution::genome::NEAT& neat) noexcept;
    void Evaluate(std::span<const double> inputs, std::span<double> outputs) noexcept;
    void Reset() noexcept;  // Clear recurrent state
};
```

**Parameters**:

- `BrainNeat(neat)`:
  - **Input**: NEAT genome table
  - **Complexity**: O(N + C) where N = node count, C = connection count
  - **Side Effects**: Builds topological order and execution plan

- `Evaluate(inputs, outputs)`:
  - **Input**: Sensor values, output span
  - **Returns**: `void` (outputs written)
  - **Complexity**: O(N + C) where N = node count, C = enabled connection count
  - **Side Effects**: Updates recurrent state buffer

- `Reset()`:
  - **Input**: None
  - **Returns**: `void`
  - **Complexity**: O(N) where N = node count
  - **Side Effects**: Clears recurrent state to zero

**Network Structure**:

- **Nodes**: Array of `NeatNode` (id, type, bias, activation)
  - Types: `Input`, `Hidden`, `Output`
- **Connections**: Array of `NeatConn` (in, out, weight, enabled, innovation, recurrent)
- **Innovation Numbers**: Historical markers for crossover alignment

**Topological Ordering**:

1. Build dependency graph from enabled connections
2. Topological sort (Kahn's algorithm)
3. Generate execution plan: array of `(dst_node, src_start, src_end)` operations
4. Cache-friendly iteration over sorted nodes

**Recurrent Edges**:

- Recurrent connections use previous-tick node values
- State buffer stores node activations from last evaluation
- `Reset()` clears state (used when entity respawns)

**Activation Functions**:

- `Linear`: `x`
- `Tanh`: `tanh(x)`
- `Relu`: `max(0, x)`
- `Sigmoid`: `1 / (1 + exp(-x))`

**Thread Safety**: Not thread-safe; each instance maintains mutable state buffers.

### BrainInferenceSystem

**Purpose**: ECS system that executes brain inference for all entities.

**Responsibilities**:
- Iterate entities with `BrainComponent` and `ActuationComponent`
- Respect `update_rate_hz` (accumulate dt, skip frames)
- Dispatch to MLP or NEAT engines based on brain kind
- Write outputs to `ActuationComponent`

**Public API**:

```cpp
class BrainInferenceSystem final : public ISystem {
public:
    explicit BrainInferenceSystem(genetics::GenomeStorage& storage) noexcept;
    void tick(SimulationContext& context) override;
    [[nodiscard]] std::string_view name() const noexcept override;
};
```

**Parameters**:

- `BrainInferenceSystem(storage)`:
  - **Input**: Reference to genome storage (must outlive system)
  - **Complexity**: O(1)
  - **Side Effects**: None (stores reference)

- `tick(context)`:
  - **Input**: Simulation context (registry, dt, time)
  - **Returns**: `void`
  - **Complexity**: O(E × W) where E = active brains, W = weights/edges per brain
  - **Side Effects**: Updates `ActuationComponent` and `BrainComponent.accum`

**Execution Flow**:

1. Iterate entities with `BrainComponent` + `ActuationComponent`
2. For each entity:
   - Accumulate `dt` into `BrainComponent.accum`
   - If `accum >= update_interval`:
     - Read sensors (energy, age, velocity, ground contact, etc.)
     - Get genome from storage
     - Dispatch to `BrainMlp::Evaluate()` or `BrainNeat::Evaluate()`
     - Write outputs to `ActuationComponent`
     - Reset `accum` to `0.0`
   - Otherwise: Increment `update_skip` counter

**Sensor Inputs** (normalized):

- `energy_frac`: `energy / max_energy` (clamped to `[0, 1]`)
- `age_frac`: `age / max_age` (clamped to `[0, 1]`)
- `vertical_speed`: `velocity.y / max_speed` (clamped to `[-1, 1]`)
- `on_ground`: `1.0` if ground contact, `0.0` otherwise
- `ground_slope_mag`: Slope magnitude `[0, 1]`
- `nearest_plant_dist`: Distance to nearest plant (normalized, capped)
- `nearest_plant_dir_xz`: Direction to plant `(cos(θ), sin(θ))`

**Output Mapping**:

- `outputs[0]` → `ActuationComponent.impulse_x` (clamped to `[-1, 1]`)
- `outputs[1]` → `ActuationComponent.impulse_z` (clamped to `[-1, 1]`)
- `outputs[2]` → `ActuationComponent.jump` (threshold `> 0.5`)
- `outputs[3]` → `ActuationComponent.eat` (threshold `> 0.5`)

**State Management**:

- `neat_slots_`: `std::vector<NeatSlot>` - Cached NEAT runtime instances
- `input_buffer_`: `std::vector<double>` - Reusable input buffer
- `output_buffer_`: `std::vector<double>` - Reusable output buffer

**Thread Safety**: Not thread-safe; operates on mutable ECS state.

## Data Contracts

### BrainInferenceSystem ↔ GenomeStorage

**Contract**: System reads genomes from storage.

**Data Flow**:
- `storage.get(genome_id)` → `const Genome*`
- System reads brain definition from genome

**Guarantees**:
- Genome pointer valid during evaluation
- Storage not mutated during tick

### BrainInferenceSystem ↔ ActuationComponent

**Contract**: System writes actuation commands.

**Data Flow**:
- Read: `BrainComponent` (update rate, accumulator)
- Write: `ActuationComponent` (impulses, jump, eat)

**Guarantees**:
- Outputs clamped to valid ranges
- Commands reset after `MotorSystem` consumes them

### BrainInferenceSystem ↔ MotorSystem

**Contract**: Brain inference runs before motor integration.

**Execution Order**:
1. `BrainInferenceSystem::tick()` - Writes actuation commands
2. `MotorSystem::tick()` - Consumes commands, applies forces

**Guarantees**:
- Actuation commands available for motor system
- Commands reset after motor consumption

## Performance Considerations

- **MLP Evaluation**: O(S) where S = weight count (cache-friendly sequential access)
- **NEAT Evaluation**: O(N + C) where N = nodes, C = connections (topological order)
- **Update Rate Control**: Skips evaluation when `accum < update_interval` (saves CPU)
- **Memory**: O(E × N) for NEAT state buffers (E = entities, N = node count per brain)

## Extension Points

### Adding New Sensor Inputs

1. Extend sensor array in `BrainInferenceSystem::tick()`
2. Update `BrainComponent.input_count` (if schema changes)
3. Ensure normalization/clamping

### Adding New Output Actions

1. Extend `ActuationComponent` with new fields
2. Map brain outputs to new fields in `BrainInferenceSystem`
3. Update `MotorSystem` to consume new actions

### Custom Activation Functions

1. Add enum value to `Activation` in `genome.fbs`
2. Implement function in `BrainMlp` or `BrainNeat`
3. Update evaluation code to dispatch to new function

## Related Documentation

- [Genome Module](./genome.md) - Genome storage and MLP/NEAT definitions
- [Motor System](./core_simulation.md#motorsystem) - Force application from actuation
- [Components Module](./components.md) - BrainComponent and ActuationComponent definitions
- [Data Contracts](../data-contracts/genetics.md) - Inter-module data flow

