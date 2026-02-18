# Tracy Profiler Agent Workflow

This document describes how to programmatically access and analyze Tracy profiler data for use by AI agents or automated analysis tools.

## Overview

Tracy captures contain binary performance data. To make this consumable by agents, we need to convert traces to a structured format (CSV or JSON) that can be parsed programmatically.

## Recommended Workflow

### Step 1: Capture Trace Data

1. Run the simulation in WSL with Tracy enabled:
   ```bash
   # Build with Tracy
   cmake -DTRACY_ENABLE=ON ..
   make -j$(( $(nproc) / 2 ))

   # Run simulation (broadcasting on port 8086)
   ./build/bin/sim_app
   ```
2. On Windows, open the **Tracy Profiler GUI**.
3. Click **"Connect"** to attach to the WSL process.
4. Capture data as needed, then click **"Save"** in the GUI.
5. **CRITICAL**: Save the trace file to your Windows Downloads folder.

### Step 2: Import & Export in WSL

Trace files saved via the Windows GUI are located at:
`/mnt/c/Users/korsz/Downloads/*.tracy`

To process a new trace, copy it into the repo and use the export tool:

```bash
# 1. Copy the latest trace from Windows Downloads
cp "/mnt/c/Users/korsz/Downloads/latest_trace.tracy" /home/karolito/evolution/trace.tracy

# 2. Run the automated export pipeline
./tools/tracy_export.sh /home/karolito/evolution/trace.tracy
```

The script `tools/tracy_export.sh` generates multiple CSV variants (summary, self-time, unwrapped) in `output/tracy/`.

### Step 3: Agent Analysis

The CSV can be parsed to identify:

**Bottlenecks**: Zones with highest `SelfTime`:
```python
bottlenecks = df.nlargest(10, 'SelfTime')
```

**Hot Paths**: Frequently called zones:
```python
hot_paths = df[df['CallCount'] > 1000]
```

**Performance Trends**: Compare `SelfTime` across simulation ticks.

## Alternative: Real-Time Metrics (Without Tracy)

For continuous agent monitoring without binary trace conversion, the simulation can emit simple JSON metrics directly:

```cpp
// In scheduler.cpp, after tick():
std::ofstream metrics("metrics.jsonl", std::ios::app);
metrics << json{
    {"tick": current_tick,
     "physics_ms": physics_time.count(),
     "feeding_ms": feeding_time.count(),
     "entity_count": registry.alive()}
}.dump() << "\n";
```

This enables:
- ✅ Real-time monitoring (tail -f metrics.jsonl)
- ✅ Simple parsing (newline-delimited JSON)
- ✅ No external tools required
- ❌ Limited detail (no call graphs)

## Current Status

- ✅ **Tracy client**: Integrated into `sim_app` build
- ✅ **Instrumentation**: Added to `simulation_app.cpp` and `scheduler.cpp`
- ⏳ **csvexport**: Build in progress (`tools/csvexport/`)
- ✅ **Trace file**: Captured example at `/home/karolito/evolution/trace.tracy`

## Next Steps

1. Complete `csvexport` build
2. Test conversion of `trace.tracy` to CSV
3. Parse CSV to validate data quality
4. (Optional) Add JSONL metrics to `scheduler.cpp` for real-time monitoring
