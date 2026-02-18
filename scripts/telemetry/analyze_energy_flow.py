#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd

if __package__ is None or __package__ == "":
    sys.path.append(str(Path(__file__).resolve().parents[1]))

from telemetry.common import ensure_out_dir, load_metrics, maybe_load_events, save_summary_json
from telemetry.energy import analyze_energy


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate energy flow analytics charts.")
    parser.add_argument("--telemetry-dir", type=Path, default=Path("output/telemetry"))
    parser.add_argument("--out-dir", type=Path, default=Path("output/reports/latest/charts/energy"))
    parser.add_argument("--run-id", type=str, default=None)
    parser.add_argument("--use-events", action="store_true")
    parser.add_argument("--rolling-window-sec", type=float, default=20.0)
    args = parser.parse_args()

    ensure_out_dir(args.out_dir)
    metrics = load_metrics(args.telemetry_dir, run_id=args.run_id)
    events = pd.DataFrame()
    if args.use_events:
        events = maybe_load_events(args.telemetry_dir, run_id=args.run_id)
    charts, summary, anomalies = analyze_energy(metrics, args.out_dir, events, args.rolling_window_sec)

    if not anomalies.empty:
        anomalies.to_csv(args.out_dir / "energy_anomalies.csv", index=False)

    save_summary_json(
        args.out_dir / "energy_summary.json",
        {"charts": charts, "summary": summary, "anomaly_count": int(len(anomalies))},
    )
    print(f"Generated {len(charts)} energy charts in {args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
