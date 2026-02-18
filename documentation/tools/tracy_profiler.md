# Tracy Profiler Integration

The simulation includes integrated support for the [Tracy Profiler](https://github.com/wolfpld/tracy). This allows for deep performance analysis of the ECS systems and task scheduler.

## Building with Tracy

To enable Tracy instrumentation, use the `TRACY_ENABLE` CMake option:

```bash
mkdir build && cd build
cmake -DTRACY_ENABLE=ON ..
make -j$(nproc)
```

## Running and Profiling

1.  **Start the Tracy Server**: Download or build the Tracy Profiler GUI (`tracy-profiler`) from the [Tracy releases](https://github.com/wolfpld/tracy/releases).
2.  **Run the Simulation**:
    ```bash
    ./bin/sim_app
    ```
3.  **Connect**: Open the Tracy GUI and click **Connect**. It should automatically detect the running `sim_app` on `localhost`.

## Instrumentation

The codebase uses the following Tracy macros:

- `ZoneScoped`: Profiles a function scope.
- `ZoneScopedN("Name")`: Profiles a scope with a custom name.
- `FrameMark`: Marks the end of a simulation tick (in `scheduler.cpp`).

Per-system zones are emitted inside each system `tick()` implementation. This ensures the CSV export aggregates per system instead of collapsing everything into a single scheduler row.

## Performance Impact

- **Disabled (Default)**: Zero runtime overhead.
- **Enabled**: Minimal overhead (nanoseconds per zone). Suitable for development and performance tuning.

## CSV Export Pipeline (WSL)

Use the `tracy-csvexport` binary to generate aggregated and per-event CSVs from a `.tracy` capture. A helper script is provided at `tools/tracy_export.sh`.

```bash
./tools/tracy_export.sh trace.tracy
```

Outputs are written to `/home/karolito/evolution/output/tracy`:

- `*.summary.csv`: Aggregated totals per zone.
- `*.summary.self.csv`: Aggregated self-time per zone.
- `*.unwrap.csv`: Per-event rows (max detail).
- `*.unwrap.self.csv`: Per-event self-time rows.
- `*.unwrap.plot.csv`: Per-event rows with plot data.

To filter by zone name:

```bash
./tools/tracy_export.sh trace.tracy /home/karolito/evolution/output/tracy tick
```

Note: csvexport aggregates by source location. Dynamic `ZoneText` names appear in the GUI but do not create separate aggregated CSV rows. Use `ZoneScopedN("SystemName")` inside system ticks to guarantee per-system CSV breakdown.
