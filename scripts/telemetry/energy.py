from __future__ import annotations

from pathlib import Path
import warnings

import matplotlib
matplotlib.use("Agg")
warnings.filterwarnings("ignore", message="Unable to import Axes3D")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

from .common import downsample_for_plot, rolling_points_for_window

sns.set_theme(style="whitegrid")


def _save(fig: plt.Figure, path: Path) -> None:
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)


def analyze_energy(
    metrics: pd.DataFrame,
    out_dir: Path,
    events: pd.DataFrame | None = None,
    rolling_window_sec: float = 20.0,
) -> tuple[list[str], dict, pd.DataFrame]:
    charts: list[str] = []
    metrics = metrics.copy()
    points = rolling_points_for_window(metrics["sim_time"], rolling_window_sec)
    minp2 = min(2, points)

    metrics["feeding_per_capita"] = (
        metrics["total_feeding_energy"] / metrics["total_population"].replace(0, np.nan)
    ).fillna(0.0)
    metrics["energy_trend"] = metrics["mean_energy"].rolling(points, min_periods=minp2).mean().bfill()
    metrics["energy_budget_signal"] = metrics["mean_energy"] + metrics["feeding_per_capita"] * 10.0

    base_df = downsample_for_plot(
        metrics[["sim_time", "mean_energy", "energy_trend"]],
        "sim_time",
    )
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.plot(base_df["sim_time"], base_df["mean_energy"], label="mean_energy", linewidth=1.0)
    ax.plot(base_df["sim_time"], base_df["energy_trend"], label="rolling_mean_energy", linewidth=1.5)
    ax.set_title("Mean Energy Over Time")
    ax.set_xlabel("Sim Time (s)")
    ax.set_ylabel("Energy")
    ax.legend()
    name = "energy_mean_and_trend.png"
    _save(fig, out_dir / name)
    charts.append(name)

    feed_df = downsample_for_plot(
        metrics[["sim_time", "total_feeding_energy", "feeding_per_capita"]],
        "sim_time",
    )
    fig, ax1 = plt.subplots(figsize=(12, 4.5))
    ax1.plot(feed_df["sim_time"], feed_df["total_feeding_energy"], color="#ff7f0e")
    ax1.set_xlabel("Sim Time (s)")
    ax1.set_ylabel("Total Feeding Energy", color="#ff7f0e")
    ax1.tick_params(axis="y", labelcolor="#ff7f0e")
    ax2 = ax1.twinx()
    ax2.plot(feed_df["sim_time"], feed_df["feeding_per_capita"], color="#2ca02c")
    ax2.set_ylabel("Feeding per Capita", color="#2ca02c")
    ax2.tick_params(axis="y", labelcolor="#2ca02c")
    ax1.set_title("Feeding Energy and Per-Capita Efficiency")
    name = "energy_feeding_efficiency.png"
    _save(fig, out_dir / name)
    charts.append(name)

    budget_df = downsample_for_plot(metrics[["sim_time", "energy_budget_signal", "death_rate"]], "sim_time")
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.plot(budget_df["sim_time"], budget_df["energy_budget_signal"], label="energy_budget_signal")
    ax.plot(budget_df["sim_time"], budget_df["death_rate"], label="death_rate")
    ax.set_title("Energy Budget Signal vs Death Rate")
    ax.set_xlabel("Sim Time (s)")
    ax.legend()
    name = "energy_budget_vs_death_rate.png"
    _save(fig, out_dir / name)
    charts.append(name)

    anomaly_rows = []
    energy_std = float(metrics["mean_energy"].std() or 0.0)
    if energy_std > 0.0:
        z = (metrics["mean_energy"] - metrics["mean_energy"].mean()) / energy_std
        stressed = metrics[z < -2.5]
        for _, row in stressed.iterrows():
            anomaly_rows.append(
                {
                    "sim_time": row["sim_time"],
                    "category": "energy_collapse_risk",
                    "value": row["mean_energy"],
                }
            )

    if events is not None and not events.empty:
        event_counts = events.groupby([events["sim_time"].astype(int), "type"]).size().unstack(fill_value=0)
        event_counts = event_counts.reset_index().rename(columns={"sim_time": "time_bucket"})
        event_counts = downsample_for_plot(event_counts, "time_bucket", max_points=1200)
        if not event_counts.empty:
            fig, ax = plt.subplots(figsize=(12, 4.5))
            for col in event_counts.columns:
                if col == "time_bucket":
                    continue
                ax.plot(event_counts["time_bucket"], event_counts[col], label=str(col), linewidth=1.0)
            ax.set_title("Event Rate by Type (1s buckets)")
            ax.set_xlabel("Sim Time Bucket (s)")
            ax.set_ylabel("Event Count")
            ax.legend(fontsize=7)
            name = "energy_event_rates.png"
            _save(fig, out_dir / name)
            charts.append(name)

    summary = {
        "mean_energy": float(metrics["mean_energy"].mean()),
        "min_energy": float(metrics["mean_energy"].min()),
        "max_energy": float(metrics["mean_energy"].max()),
        "mean_total_feeding_energy": float(metrics["total_feeding_energy"].mean()),
        "mean_feeding_per_capita": float(metrics["feeding_per_capita"].mean()),
    }
    return charts, summary, pd.DataFrame(anomaly_rows)
