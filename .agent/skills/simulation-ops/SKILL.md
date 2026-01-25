---
name: Simulation Operations
description: Running and analyzing evolution simulation experiments
---

# Simulation Operations Skill

Project-specific workflows for the evolution simulation.

## Building the Simulation

```bash
# Standard debug build
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)

# Release build (for long runs)
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)

# With sanitizers (for debugging)
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
cmake --build .
```

## Running Simulations

### Basic Run

```bash
# Run with default config
./build/bin/evolution_sim

# Run with specific config
./build/bin/evolution_sim --config configs/fast.yaml

# Run with specific seed (reproducibility)
./build/bin/evolution_sim --seed 42
```

### Long-Running Simulations

```bash
# Run in background, log output
./build/bin/evolution_sim > sim.log 2>&1 &

# Monitor progress
tail -f sim.log

# Check if still running
ps aux | grep evolution_sim

# Stop gracefully (if implemented)
kill -SIGINT $(pgrep evolution_sim)

# Force stop
kill -9 $(pgrep evolution_sim)
```

### Batch Runs

```bash
# Multiple seeds
for seed in 1 2 3 4 5; do
    ./build/bin/evolution_sim --seed $seed > results/run_$seed.log 2>&1 &
done

# Wait for all to complete
wait
echo "All runs complete"
```

## Understanding Output

### Key Metrics to Monitor

Look for these in logs/output:
- **Population size** - Should remain stable or grow
- **Generation number** - Progress indicator
- **Average fitness** - Should trend upward
- **Death rate** - High early, stabilizes later
- **Birth rate** - Indicates reproduction success

### Common Patterns

| Pattern | Meaning |
|---------|---------|
| Population crashes to 0 | Energy/food balance wrong |
| Fitness stuck | Evolution stagnated, check mutation rates |
| Rapidly growing population | Food too abundant or metabolism too low |
| Lots of deaths, few births | Environment too harsh |

## Debugging Simulation Issues

### Crash During Simulation

```bash
# Run with GDB
gdb ./build/bin/evolution_sim
(gdb) run
# ... let it crash ...
(gdb) bt          # Where did it crash?
(gdb) info locals # What was the state?
```

Common causes:
- **Segfault in update loop** - Modifying collection while iterating
- **Crash during reproduction** - Null parent pointer
- **Crash in spatial index** - Out-of-bounds coordinates

### Simulation Hangs/Stalls

```bash
# Attach GDB to running process
gdb -p $(pgrep evolution_sim)
(gdb) bt          # See where it's stuck
(gdb) info threads
```

Common causes:
- Infinite loop in brain evaluation
- Deadlock (if multithreaded)
- Very slow convergence

### Memory Issues

```bash
# Build with ASan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
cmake --build .

# Run - ASan will report issues
./build/bin/evolution_sim
```

Watch for:
- Memory leaks when creatures die (not being cleaned up)
- Use-after-free when accessing dead creatures
- Buffer overflows in neural network arrays

## Analysis Scripts

### Python Analysis (scripts/)

```bash
# Ensure dependencies
pip install matplotlib numpy pandas

# Run analysis
cd scripts
python energy_flux.py ../results/latest/
python lineage_graph.py ../results/latest/
python trait_plots.py ../results/latest/
```

### Common Analyses

```bash
# Population over time
python -c "
import pandas as pd
import matplotlib.pyplot as plt
df = pd.read_csv('results/stats.csv')
df.plot(x='generation', y='population')
plt.savefig('population.png')
"

# Fitness distribution
python -c "
import pandas as pd
df = pd.read_csv('results/stats.csv')
print(df.describe())
"
```

## Key Components Reference

### Core Systems

| System | File | Purpose |
|--------|------|---------|
| Evolution | `evolution_system.cpp` | Handles selection, reproduction |
| Metabolism | `metabolism_system.h` | Energy consumption/death |
| Reproduction | `reproduction_system.h` | Breeding logic |
| Decomposition | `decomposition_system.cpp` | Dead creature cleanup |

### Genetics

| Component | File | Purpose |
|-----------|------|---------|
| Mutation | `mutation_ops.h` | Genetic mutations |
| Phenotype | `phenotype_builder.cpp` | Genes → traits |
| Morphology | `morphology_ops.cpp` | Body plan genes |
| Innovation | `innovation_database.h` | Tracking new genes |

### Spatial

| Component | File | Purpose |
|-----------|------|---------|
| Spatial Index | `creature_spatial_index.h` | Fast lookups |
| Trait Analysis | `trait_analysis_system.cpp` | Trait distribution |

## Experiment Workflow

1. **Hypothesis** - What behavior do you expect?
2. **Configure** - Set parameters in config file
3. **Run** - Execute with logging: `./sim > exp.log 2>&1`
4. **Monitor** - Watch logs for issues: `tail -f exp.log`
5. **Analyze** - Run analysis scripts
6. **Document** - Record findings
7. **Iterate** - Adjust parameters, repeat
