# Telemetry Visualization Scripts

Offline analytics for simulation telemetry outputs.

## Inputs

Expected files under `output/telemetry/`:
- `metrics.csv`
- `species_rollups.csv`
- `events.jsonl` (optional; only when `--use-events` is enabled)

## One-command report

```bash
python3 scripts/telemetry/generate_report.py \
  --telemetry-dir output/telemetry \
  --out-dir output/reports/latest \
  --rolling-window-sec 20
```

Artifacts:
- `output/reports/latest/index.html`
- `output/reports/latest/summary.json`
- `output/reports/latest/anomalies.csv`
- `output/reports/latest/charts/**/*.png`

## Invariant checks

Validate one run:

```bash
python3 scripts/telemetry/check_invariants.py \
  --telemetry-dir output/telemetry \
  --preset default \
  --out-json output/telemetry/invariants.json \
  --min-pop 100 \
  --max-pop 1200
```

Aggregate matrix results:

```bash
python3 scripts/telemetry/summarize_matrix.py \
  --matrix-dir output/stability_runs/smoke_matrix \
  --out-json output/stability_runs/smoke_matrix/matrix_summary.json
```

## Stability matrix runner

```bash
SIM_STEPS=10800 ./tools/run_stability_matrix.sh
```

Per preset it generates:
- `sim.log`
- `telemetry/*.csv`
- `report/index.html`
- `invariants.json`

Matrix-level:
- `matrix_summary.json`

## Individual analyzers

```bash
python3 scripts/telemetry/analyze_population.py --telemetry-dir output/telemetry
python3 scripts/telemetry/analyze_species.py --telemetry-dir output/telemetry
python3 scripts/telemetry/analyze_energy_flow.py --telemetry-dir output/telemetry
```

## Optional event parsing

`events.jsonl` can be large. Event parsing is disabled by default.

```bash
python3 scripts/telemetry/generate_report.py --use-events
```

## Dependencies

- Python 3.10+
- `pandas`
- `numpy`
- `matplotlib`
- `seaborn`
