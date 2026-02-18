#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

OUT_ROOT="${1:-output/stability_runs/$(date +%F_%H-%M-%S)}"
STEPS="${SIM_STEPS:-10800}"
MIN_POP_COMPLIANCE="${MIN_POP_COMPLIANCE:-0.80}"
MAX_DROP_RATE="${MAX_DROP_RATE:-0.95}"
PRESETS=(
  "stability_low_resources"
  "stability_high_density"
  "stability_predator_pressure"
)

MIN_STEPS_FOR_ROLLUPS=240
if (( STEPS < MIN_STEPS_FOR_ROLLUPS )); then
  echo "[stability-matrix] SIM_STEPS=$STEPS too low for reliable rollups; using $MIN_STEPS_FOR_ROLLUPS"
  STEPS="$MIN_STEPS_FOR_ROLLUPS"
fi

mkdir -p "$OUT_ROOT"
cmake --build build --target sim_app -j$(( $(nproc) / 2 ))

for preset in "${PRESETS[@]}"; do
  run_dir="$OUT_ROOT/$preset"
  mkdir -p "$run_dir"
  echo "[stability-matrix] running preset=$preset steps=$STEPS out=$run_dir"
  SIM_PRESET="$preset" \
  SIM_STEPS="$STEPS" \
  SIM_OUTPUT_DIR="$run_dir" \
  ./build/bin/sim_app >"$run_dir/sim.log" 2>&1

  if ! MPLCONFIGDIR="/tmp/matplotlib-cache" python3 scripts/telemetry/generate_report.py \
    --telemetry-dir "$run_dir/telemetry" \
    --out-dir "$run_dir/report" \
    --rolling-window-sec 20 >"$run_dir/report_generation.log" 2>&1; then
    echo "[stability-matrix] report generation failed for preset=$preset (see $run_dir/report_generation.log)"
  fi

  min_pop=100
  max_pop=1200
  case "$preset" in
    stability_low_resources)
      min_pop=80
      max_pop=1000
      ;;
    stability_high_density)
      min_pop=200
      max_pop=2000
      ;;
    stability_predator_pressure)
      min_pop=100
      max_pop=1300
      ;;
  esac

  if ! python3 scripts/telemetry/check_invariants.py \
    --telemetry-dir "$run_dir/telemetry" \
    --preset "$preset" \
    --out-json "$run_dir/invariants.json" \
    --min-pop "$min_pop" \
    --max-pop "$max_pop" \
    --min-pop-compliance "$MIN_POP_COMPLIANCE" \
    --max-drop-rate "$MAX_DROP_RATE"; then
    echo "[stability-matrix] invariant check failed for preset=$preset"
  fi

  echo "[stability-matrix] completed preset=$preset"
done

python3 scripts/telemetry/summarize_matrix.py \
  --matrix-dir "$OUT_ROOT" \
  --out-json "$OUT_ROOT/matrix_summary.json"

echo "[stability-matrix] all runs complete: $OUT_ROOT"
