#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
import pandas as pd


@dataclass
class CheckResult:
    name: str
    passed: bool
    details: str


def _is_numeric_series_finite(series: pd.Series) -> bool:
    values = pd.to_numeric(series, errors="coerce")
    return bool(np.isfinite(values.to_numpy(dtype=float)).all())


def _append(results: list[CheckResult], name: str, passed: bool, details: str) -> None:
    results.append(CheckResult(name=name, passed=passed, details=details))


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate simulation telemetry invariants.")
    parser.add_argument("--telemetry-dir", type=Path, required=True)
    parser.add_argument("--preset", type=str, required=True)
    parser.add_argument("--out-json", type=Path, required=True)
    parser.add_argument("--min-pop", type=float, required=True)
    parser.add_argument("--max-pop", type=float, required=True)
    parser.add_argument("--min-pop-compliance", type=float, default=0.85)
    parser.add_argument("--max-drop-rate", type=float, default=0.95)
    args = parser.parse_args()

    metrics_path = args.telemetry_dir / "metrics.csv"
    results: list[CheckResult] = []

    if not metrics_path.exists():
        _append(results, "metrics_file_exists", False, f"Missing file: {metrics_path}")
        payload = {
            "preset": args.preset,
            "passed": False,
            "checks": [asdict(r) for r in results],
            "summary": {"reason": "missing_metrics"},
        }
        args.out_json.parent.mkdir(parents=True, exist_ok=True)
        args.out_json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"[invariants] {args.preset}: FAIL (missing metrics.csv)")
        return 2

    frame = pd.read_csv(metrics_path)
    if frame.empty:
        _append(results, "metrics_non_empty", False, "metrics.csv has no rows")
        payload = {
            "preset": args.preset,
            "passed": False,
            "checks": [asdict(r) for r in results],
            "summary": {"reason": "empty_metrics"},
        }
        args.out_json.parent.mkdir(parents=True, exist_ok=True)
        args.out_json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"[invariants] {args.preset}: FAIL (empty metrics.csv)")
        return 2

    required_cols = [
        "sim_time",
        "total_population",
        "mean_energy",
        "total_feeding_energy",
        "reproduction_rate",
        "death_rate",
        "population_stability_index",
        "rescue_count",
        "cull_count",
        "events_dropped",
        "events_written",
        "effective_sampling_rate",
    ]
    missing = [c for c in required_cols if c not in frame.columns]
    _append(results, "required_columns_present", len(missing) == 0, "missing=" + ",".join(missing) if missing else "ok")

    if missing:
        payload = {
            "preset": args.preset,
            "passed": False,
            "checks": [asdict(r) for r in results],
            "summary": {"reason": "missing_columns", "missing": missing},
        }
        args.out_json.parent.mkdir(parents=True, exist_ok=True)
        args.out_json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"[invariants] {args.preset}: FAIL (missing columns)")
        return 2

    numeric_cols = [c for c in frame.columns if c not in {"run_id"}]
    finite_ok = all(_is_numeric_series_finite(frame[c]) for c in numeric_cols)
    _append(results, "all_numeric_values_finite", finite_ok, "all finite" if finite_ok else "found NaN/inf")

    monotonic_time = bool(frame["sim_time"].is_monotonic_increasing)
    _append(results, "sim_time_monotonic", monotonic_time, "monotonic increasing" if monotonic_time else "time regression detected")

    non_negative_cols = [
        "total_population",
        "mean_energy",
        "total_feeding_energy",
        "reproduction_rate",
        "death_rate",
        "population_stability_index",
        "rescue_count",
        "cull_count",
        "events_dropped",
        "events_written",
    ]
    non_negative_ok = True
    for col in non_negative_cols:
        if (frame[col] < 0).any():
            non_negative_ok = False
            break
    _append(results, "non_negative_core_metrics", non_negative_ok, "all >= 0" if non_negative_ok else "negative values found")

    in_window = (frame["total_population"] >= args.min_pop) & (frame["total_population"] <= args.max_pop)
    compliance = float(in_window.mean()) if len(frame) > 0 else 0.0
    compliance_ok = compliance >= args.min_pop_compliance
    _append(
        results,
        "population_window_compliance",
        compliance_ok,
        f"compliance={compliance:.3f}, required>={args.min_pop_compliance:.3f}, window=[{args.min_pop},{args.max_pop}]",
    )

    rescue_monotonic = bool(frame["rescue_count"].is_monotonic_increasing)
    cull_monotonic = bool(frame["cull_count"].is_monotonic_increasing)
    _append(results, "rescue_counter_monotonic", rescue_monotonic, "ok" if rescue_monotonic else "counter decreased")
    _append(results, "cull_counter_monotonic", cull_monotonic, "ok" if cull_monotonic else "counter decreased")

    drop_rate = 0.0
    if frame["events_written"].max() > 0:
        latest_written = float(frame["events_written"].iloc[-1])
        latest_dropped = float(frame["events_dropped"].iloc[-1])
        denom = latest_written + latest_dropped
        drop_rate = (latest_dropped / denom) if denom > 0 else 0.0
    drop_ok = drop_rate <= args.max_drop_rate
    _append(results, "event_drop_rate_bounded", drop_ok, f"drop_rate={drop_rate:.3f}, limit={args.max_drop_rate:.3f}")

    sample_rate_ok = bool(((frame["effective_sampling_rate"] >= 0.0) & (frame["effective_sampling_rate"] <= 1.0)).all())
    _append(results, "effective_sampling_rate_in_unit_interval", sample_rate_ok, "within [0,1]" if sample_rate_ok else "out of bounds")

    passed = all(item.passed for item in results)
    summary = {
        "rows": int(len(frame)),
        "sim_time_start": float(frame["sim_time"].iloc[0]),
        "sim_time_end": float(frame["sim_time"].iloc[-1]),
        "population_min": float(frame["total_population"].min()),
        "population_max": float(frame["total_population"].max()),
        "population_compliance": compliance,
        "drop_rate": drop_rate,
        "rescues_total": int(frame["rescue_count"].max()),
        "culls_total": int(frame["cull_count"].max()),
    }

    payload = {
        "preset": args.preset,
        "passed": passed,
        "checks": [asdict(r) for r in results],
        "summary": summary,
        "inputs": {
            "min_pop": args.min_pop,
            "max_pop": args.max_pop,
            "min_pop_compliance": args.min_pop_compliance,
            "max_drop_rate": args.max_drop_rate,
        },
    }

    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    args.out_json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"[invariants] {args.preset}: {'PASS' if passed else 'FAIL'}")
    return 0 if passed else 2


if __name__ == "__main__":
    raise SystemExit(main())
