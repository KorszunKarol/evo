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


def analyze_population(
    metrics: pd.DataFrame,
    out_dir: Path,
    rolling_window_sec: float = 20.0,
) -> tuple[list[str], dict, pd.DataFrame]:
    charts: list[str] = []
    metrics = metrics.copy()
    points = rolling_points_for_window(metrics["sim_time"], rolling_window_sec)
    minp2 = min(2, points)
    minp3 = min(3, points)

    metrics["population_delta"] = metrics["total_population"].diff().fillna(0.0)
    metrics["volatility"] = metrics["total_population"].rolling(points, min_periods=minp2).std().fillna(0.0)
    rolling_max = metrics["total_population"].rolling(points, min_periods=minp3).max().replace(0, np.nan)
    metrics["capacity_pressure"] = (metrics["total_population"] / rolling_max).fillna(0.0)
    metrics["recovery_signal"] = (
        metrics["population_delta"].rolling(points, min_periods=minp2).mean().fillna(0.0)
    )

    pop_plot_df = downsample_for_plot(metrics[["sim_time", "total_population"]], "sim_time")
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.plot(pop_plot_df["sim_time"], pop_plot_df["total_population"], color="#1f77b4", linewidth=1.4)
    ax.set_title("Total Population Over Time")
    ax.set_xlabel("Sim Time (s)")
    ax.set_ylabel("Population")
    name = "population_total_over_time.png"
    _save(fig, out_dir / name)
    charts.append(name)

    rate_plot_df = downsample_for_plot(
        metrics[["sim_time", "reproduction_rate", "death_rate"]],
        "sim_time",
    )
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.plot(rate_plot_df["sim_time"], rate_plot_df["reproduction_rate"], label="reproduction_rate", linewidth=1.2)
    ax.plot(rate_plot_df["sim_time"], rate_plot_df["death_rate"], label="death_rate", linewidth=1.2)
    ax.set_title("Reproduction vs Death Rates")
    ax.set_xlabel("Sim Time (s)")
    ax.set_ylabel("Rate")
    ax.legend()
    name = "population_rates.png"
    _save(fig, out_dir / name)
    charts.append(name)

    pressure_df = downsample_for_plot(metrics[["sim_time", "capacity_pressure", "volatility"]], "sim_time")
    fig, ax1 = plt.subplots(figsize=(12, 4.5))
    ax1.plot(pressure_df["sim_time"], pressure_df["capacity_pressure"], color="#d62728", label="capacity_pressure")
    ax1.set_xlabel("Sim Time (s)")
    ax1.set_ylabel("Capacity Pressure", color="#d62728")
    ax1.tick_params(axis="y", labelcolor="#d62728")
    ax2 = ax1.twinx()
    ax2.plot(pressure_df["sim_time"], pressure_df["volatility"], color="#2ca02c", label="volatility")
    ax2.set_ylabel("Population Volatility", color="#2ca02c")
    ax2.tick_params(axis="y", labelcolor="#2ca02c")
    ax1.set_title("Capacity Pressure and Population Volatility")
    name = "population_pressure_volatility.png"
    _save(fig, out_dir / name)
    charts.append(name)

    anomaly_rows = []
    delta_std = float(metrics["population_delta"].std() or 0.0)
    if delta_std > 0.0:
        z = (metrics["population_delta"] - metrics["population_delta"].mean()) / delta_std
        crashes = metrics[z < -3.0]
        spikes = metrics[z > 3.0]
        for _, row in crashes.iterrows():
            anomaly_rows.append({"sim_time": row["sim_time"], "category": "population_crash", "value": row["population_delta"]})
        for _, row in spikes.iterrows():
            anomaly_rows.append({"sim_time": row["sim_time"], "category": "population_spike", "value": row["population_delta"]})

    metrics["energy_stress"] = (
        (metrics["mean_energy"].rolling(points, min_periods=minp2).mean().max() - metrics["mean_energy"]) 
        * (1.0 + metrics["death_rate"].clip(lower=0.0))
    )
    stress_df = downsample_for_plot(metrics[["sim_time", "energy_stress"]], "sim_time")
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.plot(stress_df["sim_time"], stress_df["energy_stress"], color="#9467bd", linewidth=1.3)
    ax.set_title("Energy Stress Index")
    ax.set_xlabel("Sim Time (s)")
    ax.set_ylabel("Stress (relative)")
    name = "population_energy_stress.png"
    _save(fig, out_dir / name)
    charts.append(name)

    summary = {
        "max_population": int(metrics["total_population"].max()),
        "min_population": int(metrics["total_population"].min()),
        "mean_population": float(metrics["total_population"].mean()),
        "mean_reproduction_rate": float(metrics["reproduction_rate"].mean()),
        "mean_death_rate": float(metrics["death_rate"].mean()),
        "max_capacity_pressure": float(metrics["capacity_pressure"].max()),
        "max_volatility": float(metrics["volatility"].max()),
    }

    anomaly_df = pd.DataFrame(anomaly_rows)
    return charts, summary, anomaly_df
