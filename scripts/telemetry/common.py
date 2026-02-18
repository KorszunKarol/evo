from __future__ import annotations

import json
from pathlib import Path
from typing import Iterable

import numpy as np
import pandas as pd

REQUIRED_METRICS_COLUMNS = {
    "schema_version",
    "run_id",
    "sim_time",
    "total_population",
    "mean_energy",
    "total_feeding_energy",
    "reproduction_rate",
    "death_rate",
    "population_stability_index",
    "rescue_count",
    "cull_count",
}

REQUIRED_SPECIES_COLUMNS = {
    "schema_version",
    "run_id",
    "sim_time",
    "species_id",
    "population",
    "mean_energy",
}


def ensure_out_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def _validate_columns(frame: pd.DataFrame, required: set[str], name: str) -> None:
    missing = sorted(required.difference(frame.columns))
    if missing:
        raise ValueError(f"{name} is missing required columns: {', '.join(missing)}")


def load_metrics(telemetry_dir: Path, run_id: str | None = None) -> pd.DataFrame:
    metrics_path = telemetry_dir / "metrics.csv"
    if not metrics_path.exists():
        raise FileNotFoundError(f"Missing telemetry file: {metrics_path}")

    frame = pd.read_csv(metrics_path)
    _validate_columns(frame, REQUIRED_METRICS_COLUMNS, "metrics.csv")

    if run_id:
        frame = frame[frame["run_id"] == run_id]
    if frame.empty:
        raise ValueError("No metric rows found for the selected run_id.")

    frame = frame.sort_values("sim_time").reset_index(drop=True)
    return frame


def load_species_rollups(telemetry_dir: Path, run_id: str | None = None) -> pd.DataFrame:
    path = telemetry_dir / "species_rollups.csv"
    if not path.exists():
        raise FileNotFoundError(f"Missing telemetry file: {path}")

    frame = pd.read_csv(path)
    _validate_columns(frame, REQUIRED_SPECIES_COLUMNS, "species_rollups.csv")

    if run_id:
        frame = frame[frame["run_id"] == run_id]
    if frame.empty:
        raise ValueError("No species rollup rows found for the selected run_id.")

    frame = frame.sort_values(["sim_time", "species_id"]).reset_index(drop=True)
    return frame


def maybe_load_events(
    telemetry_dir: Path,
    run_id: str | None = None,
    allowed_types: Iterable[str] | None = None,
) -> pd.DataFrame:
    path = telemetry_dir / "events.jsonl"
    if not path.exists():
        return pd.DataFrame()

    allowed = set(allowed_types or [])
    rows: list[dict] = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            if run_id and record.get("run_id") != run_id:
                continue
            event_type = record.get("type")
            if allowed and event_type not in allowed:
                continue
            payload = record.get("payload") if isinstance(record.get("payload"), dict) else {}
            rows.append(
                {
                    "run_id": record.get("run_id"),
                    "sim_time": float(record.get("sim_time", 0.0)),
                    "type": event_type,
                    **payload,
                }
            )

    if not rows:
        return pd.DataFrame()
    return pd.DataFrame(rows).sort_values("sim_time").reset_index(drop=True)


def rolling_points_for_window(sim_time: pd.Series, window_seconds: float) -> int:
    if len(sim_time) < 3:
        return 1
    deltas = np.diff(sim_time.to_numpy(dtype=float))
    deltas = deltas[deltas > 0]
    if deltas.size == 0:
        return 1
    dt = float(np.median(deltas))
    if dt <= 0:
        return 1
    return max(1, int(round(window_seconds / dt)))


def downsample_for_plot(frame: pd.DataFrame, x_col: str, max_points: int = 2500) -> pd.DataFrame:
    if len(frame) <= max_points:
        return frame
    indexes = np.linspace(0, len(frame) - 1, num=max_points, dtype=int)
    return frame.iloc[indexes].sort_values(x_col)


def save_summary_json(path: Path, payload: dict) -> None:
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
