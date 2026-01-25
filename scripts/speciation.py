#!/usr/bin/env python3
"""
Speciation Analysis for Evolution Simulation.

Performs clustering on genome feature vectors to detect species emergence.
Generates species abundance plots and genetic distance visualizations.

Usage:
    python3 speciation.py --input telemetry/speciation.csv --output speciation_plots/

Requirements:
    pip install pandas matplotlib scikit-learn seaborn
"""

import argparse
from pathlib import Path
from collections import defaultdict

import numpy as np


def load_speciation_csv(filepath: Path):
    """Load speciation.csv into arrays."""
    import pandas as pd
    
    df = pd.read_csv(filepath)
    
    times = df['time'].values
    genome_ids = df['genome_id'].values
    
    feature_cols = [c for c in df.columns if c.startswith('f')]
    features = df[feature_cols].values
    
    return times, genome_ids, features


def cluster_genomes(features: np.ndarray, method: str = 'dbscan') -> np.ndarray:
    """Cluster genomes into species using DBSCAN or K-Means."""
    from sklearn.cluster import DBSCAN, KMeans
    from sklearn.preprocessing import StandardScaler
    
    scaler = StandardScaler()
    features_scaled = scaler.fit_transform(features)
    
    if method == 'dbscan':
        clusterer = DBSCAN(eps=0.5, min_samples=3)
    else:
        n_clusters = min(10, len(features) // 10)
        n_clusters = max(2, n_clusters)
        clusterer = KMeans(n_clusters=n_clusters, random_state=42, n_init=10)
    
    labels = clusterer.fit_predict(features_scaled)
    return labels


def plot_species_tsne(features: np.ndarray, labels: np.ndarray, output_path: Path) -> None:
    """Plot t-SNE of genomes colored by species."""
    try:
        from sklearn.manifold import TSNE
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError as e:
        print(f"Missing dependency: {e}")
        return
    
    if len(features) < 5:
        print("Not enough samples for t-SNE.")
        return
    
    if len(features) > 2000:
        indices = np.random.choice(len(features), 2000, replace=False)
        features = features[indices]
        labels = labels[indices]
    
    print(f"Running t-SNE on {len(features)} samples...")
    perplexity = min(30, len(features) - 1)
    tsne = TSNE(n_components=2, random_state=42, perplexity=perplexity)
    embedding = tsne.fit_transform(features)
    
    unique_labels = set(labels)
    colors = plt.cm.tab20(np.linspace(0, 1, len(unique_labels)))
    color_map = {label: colors[i] for i, label in enumerate(unique_labels)}
    
    fig, ax = plt.subplots(figsize=(12, 10))
    
    for label in unique_labels:
        mask = labels == label
        if label == -1:
            ax.scatter(embedding[mask, 0], embedding[mask, 1], 
                      c='gray', alpha=0.3, s=10, label='Noise')
        else:
            ax.scatter(embedding[mask, 0], embedding[mask, 1],
                      c=[color_map[label]], alpha=0.6, s=20, label=f'Species {label}')
    
    ax.legend(loc='upper right', fontsize=8, ncol=2)
    ax.set_xlabel('t-SNE Dimension 1')
    ax.set_ylabel('t-SNE Dimension 2')
    ax.set_title('Genome Clustering (Species)')
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Saved: {output_path}")


def plot_muller_diagram(times: np.ndarray, labels: np.ndarray, output_path: Path) -> None:
    """Plot Muller diagram showing species abundance over time."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        return
    
    unique_times = sorted(set(times))
    unique_species = sorted(set(labels[labels >= 0]))
    
    if len(unique_species) == 0:
        print("No valid species clusters for Muller diagram.")
        return
    
    abundance = defaultdict(lambda: defaultdict(int))
    for t, label in zip(times, labels):
        if label >= 0:
            abundance[t][label] += 1
    
    time_points = []
    species_counts = {sp: [] for sp in unique_species}
    
    for t in unique_times:
        time_points.append(t)
        total = sum(abundance[t].values()) or 1
        for sp in unique_species:
            species_counts[sp].append(abundance[t].get(sp, 0) / total)
    
    fig, ax = plt.subplots(figsize=(14, 6))
    
    y_stack = np.zeros(len(time_points))
    colors = plt.cm.tab20(np.linspace(0, 1, len(unique_species)))
    
    for i, sp in enumerate(unique_species):
        y_next = y_stack + np.array(species_counts[sp])
        ax.fill_between(time_points, y_stack, y_next, color=colors[i], 
                        alpha=0.8, label=f'Species {sp}')
        y_stack = y_next
    
    ax.set_xlabel('Simulation Time (s)')
    ax.set_ylabel('Relative Abundance')
    ax.set_title('Species Abundance Over Time (Muller Diagram)')
    ax.legend(loc='upper right', fontsize=8, ncol=2)
    ax.set_ylim(0, 1)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Saved: {output_path}")


def compute_diversity_index(times: np.ndarray, labels: np.ndarray) -> dict:
    """Compute Shannon Diversity Index over time."""
    unique_times = sorted(set(times))
    diversity = {}
    
    for t in unique_times:
        mask = times == t
        t_labels = labels[mask]
        t_labels = t_labels[t_labels >= 0]
        
        if len(t_labels) == 0:
            diversity[t] = 0.0
            continue
        
        unique, counts = np.unique(t_labels, return_counts=True)
        probabilities = counts / counts.sum()
        
        shannon = -np.sum(probabilities * np.log(probabilities + 1e-10))
        diversity[t] = shannon
    
    return diversity


def plot_diversity_over_time(diversity: dict, output_path: Path) -> None:
    """Plot Shannon Diversity Index over time."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        return
    
    times = sorted(diversity.keys())
    values = [diversity[t] for t in times]
    
    fig, ax = plt.subplots(figsize=(12, 4))
    ax.plot(times, values, 'b-', linewidth=2)
    ax.fill_between(times, 0, values, alpha=0.3)
    
    ax.set_xlabel('Simulation Time (s)')
    ax.set_ylabel('Shannon Diversity Index')
    ax.set_title('Species Diversity Over Time')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(description='Analyze speciation from genome features.')
    parser.add_argument('--input', '-i', type=str, default='telemetry/speciation.csv',
                        help='Path to speciation.csv')
    parser.add_argument('--output', '-o', type=str, default='speciation_plots',
                        help='Output directory')
    parser.add_argument('--method', type=str, choices=['dbscan', 'kmeans'], default='dbscan',
                        help='Clustering method')
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1
    
    print(f"Loading speciation data from: {input_path}")
    times, genome_ids, features = load_speciation_csv(input_path)
    print(f"Loaded {len(features)} genome snapshots with {features.shape[1]} features")
    
    if len(features) < 5:
        print("Not enough data for clustering.")
        return 0
    
    print(f"Clustering with {args.method}...")
    labels = cluster_genomes(features, args.method)
    
    n_species = len(set(labels[labels >= 0]))
    n_noise = np.sum(labels == -1)
    print(f"Found {n_species} species, {n_noise} noise points")
    
    plot_species_tsne(features, labels, output_dir / 'species_tsne.png')
    plot_muller_diagram(times, labels, output_dir / 'muller_diagram.png')
    
    diversity = compute_diversity_index(times, labels)
    plot_diversity_over_time(diversity, output_dir / 'diversity_index.png')
    
    print("Speciation analysis complete.")
    return 0


if __name__ == '__main__':
    exit(main())
