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

from .common import downsample_for_plot

sns.set_theme(style="whitegrid")


def _save(fig: plt.Figure, path: Path) -> None:
    fig.tight_layout()
    fig.savefig(path, dpi=160)
    plt.close(fig)


def _safe_entropy(group: pd.Series) -> float:
    vals = group.to_numpy(dtype=float)
    total = vals.sum()
    if total <= 0:
        return 0.0
    p = vals / total
    p = p[p > 0]
    if p.size == 0:
        return 0.0
    return float(-(p * np.log(p)).sum())


def analyze_species(species: pd.DataFrame, out_dir: Path) -> tuple[list[str], dict, pd.DataFrame]:
    charts: list[str] = []
    species = species.copy()

    per_time = species.groupby("sim_time", as_index=False).agg(
        active_species=("species_id", "nunique"),
        total_population=("population", "sum"),
        mean_energy=("mean_energy", "mean"),
    )

    max_by_time = species.groupby(["sim_time", "species_id"], as_index=False)["population"].sum()
    dominant = max_by_time.groupby("sim_time", as_index=False)["population"].max().rename(columns={"population": "dominant_population"})
    per_time = per_time.merge(dominant, on="sim_time", how="left")
    per_time["dominance_ratio"] = (
        per_time["dominant_population"] / per_time["total_population"].replace(0, np.nan)
    ).fillna(0.0)

    shannon = species.groupby("sim_time")["population"].apply(_safe_entropy).reset_index(name="shannon_diversity")
    simpson = (
        species.groupby("sim_time")["population"]
        .apply(lambda g: float(1.0 - np.square(g / g.sum()).sum()) if g.sum() > 0 else 0.0)
        .reset_index(name="simpson_diversity")
    )
    per_time = per_time.merge(shannon, on="sim_time", how="left").merge(simpson, on="sim_time", how="left")

    species_count_df = downsample_for_plot(per_time[["sim_time", "active_species"]], "sim_time")
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.plot(species_count_df["sim_time"], species_count_df["active_species"], color="#1f77b4")
    ax.set_title("Active Species Over Time")
    ax.set_xlabel("Sim Time (s)")
    ax.set_ylabel("Species Count")
    name = "species_active_count.png"
    _save(fig, out_dir / name)
    charts.append(name)

    diversity_df = downsample_for_plot(
        per_time[["sim_time", "shannon_diversity", "simpson_diversity", "dominance_ratio"]],
        "sim_time",
    )
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.plot(diversity_df["sim_time"], diversity_df["shannon_diversity"], label="shannon")
    ax.plot(diversity_df["sim_time"], diversity_df["simpson_diversity"], label="simpson")
    ax.plot(diversity_df["sim_time"], diversity_df["dominance_ratio"], label="dominance_ratio")
    ax.set_title("Diversity and Dominance Metrics")
    ax.set_xlabel("Sim Time (s)")
    ax.set_ylabel("Index")
    ax.legend()
    name = "species_diversity_dominance.png"
    _save(fig, out_dir / name)
    charts.append(name)

    pop_by_species = (
        species.groupby(["sim_time", "species_id"], as_index=False)["population"].sum().pivot(
            index="sim_time", columns="species_id", values="population"
        ).fillna(0.0)
    )
    top_species = pop_by_species.max().sort_values(ascending=False).head(8).index.tolist()
    area_df = pop_by_species[top_species].copy() if top_species else pd.DataFrame(index=pop_by_species.index)
    if not area_df.empty:
        downsampled = downsample_for_plot(area_df.reset_index(), "sim_time", max_points=1200).set_index("sim_time")
        fig, ax = plt.subplots(figsize=(12, 6))
        ax.stackplot(
            downsampled.index.to_numpy(),
            *[downsampled[col].to_numpy() for col in downsampled.columns],
            labels=[str(c) for c in downsampled.columns],
            alpha=0.85,
        )
        ax.set_title("Top Species Population Share (Stacked)")
        ax.set_xlabel("Sim Time (s)")
        ax.set_ylabel("Population")
        ax.legend(loc="upper left", fontsize=7)
        name = "species_top_share_stacked.png"
        _save(fig, out_dir / name)
        charts.append(name)

    species_presence = (pop_by_species > 0).astype(int)
    births = species_presence.diff().fillna(species_presence).eq(1).sum(axis=1)
    extinctions = species_presence.diff().fillna(0).eq(-1).sum(axis=1)
    turnover = pd.DataFrame(
        {
            "sim_time": species_presence.index.to_numpy(dtype=float),
            "species_births": births.to_numpy(dtype=float),
            "species_extinctions": extinctions.to_numpy(dtype=float),
        }
    )

    turnover_plot = downsample_for_plot(turnover, "sim_time")
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.plot(turnover_plot["sim_time"], turnover_plot["species_births"], label="species_births")
    ax.plot(turnover_plot["sim_time"], turnover_plot["species_extinctions"], label="species_extinctions")
    ax.set_title("Species Turnover Events Over Time")
    ax.set_xlabel("Sim Time (s)")
    ax.set_ylabel("Event Count")
    ax.legend()
    name = "species_turnover.png"
    _save(fig, out_dir / name)
    charts.append(name)

    summary = {
        "peak_active_species": int(per_time["active_species"].max()),
        "mean_active_species": float(per_time["active_species"].mean()),
        "mean_shannon_diversity": float(per_time["shannon_diversity"].mean()),
        "mean_dominance_ratio": float(per_time["dominance_ratio"].mean()),
        "total_species_birth_events": float(turnover["species_births"].sum()),
        "total_species_extinction_events": float(turnover["species_extinctions"].sum()),
    }

    anomaly_rows = []
    if not per_time.empty:
        high_dominance = per_time[per_time["dominance_ratio"] > 0.85]
        for _, row in high_dominance.iterrows():
            anomaly_rows.append(
                {
                    "sim_time": row["sim_time"],
                    "category": "species_monoculture_risk",
                    "value": row["dominance_ratio"],
                }
            )

    return charts, summary, pd.DataFrame(anomaly_rows)
