#!/usr/bin/env python3
"""
Neural Manifold Visualization for Evolution Simulation.

Uses UMAP to reduce high-dimensional brain activations to 2D/3D
for visualization of agent "thought space".

Usage:
    python3 brain_manifold.py --input telemetry/brain_activations.csv --output manifold.png

Requirements:
    pip install umap-learn matplotlib pandas scikit-learn
"""

import argparse
from pathlib import Path

import numpy as np


def load_activations(filepath: Path):
    """Load brain_activations.csv into arrays."""
    import pandas as pd
    
    df = pd.read_csv(filepath)
    
    time = df['time'].values
    entity_ids = df['entity_id'].values
    action_masks = df['action_mask'].values
    
    feature_cols = [c for c in df.columns if c.startswith('v')]
    features = df[feature_cols].values
    
    return time, entity_ids, action_masks, features


def action_mask_to_color(mask: int) -> str:
    """Convert action bitmask to color."""
    eat = mask & 1
    jump = (mask >> 1) & 1
    attack = (mask >> 2) & 1
    
    if attack:
        return 'red'
    elif eat:
        return 'green'
    elif jump:
        return 'blue'
    else:
        return 'gray'


def plot_manifold_umap(features: np.ndarray, 
                       action_masks: np.ndarray,
                       output_path: Path,
                       n_neighbors: int = 15,
                       min_dist: float = 0.1) -> None:
    """Apply UMAP and plot 2D manifold."""
    try:
        import umap
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError as e:
        print(f"Missing dependency: {e}")
        print("Install with: pip install umap-learn matplotlib")
        return
    
    valid_mask = np.any(features != 0, axis=1)
    if np.sum(valid_mask) < 10:
        print("Not enough non-zero samples for UMAP.")
        return
    
    features = features[valid_mask]
    action_masks = action_masks[valid_mask]
    
    if len(features) > 5000:
        indices = np.random.choice(len(features), 5000, replace=False)
        features = features[indices]
        action_masks = action_masks[indices]
    
    print(f"Running UMAP on {len(features)} samples...")
    reducer = umap.UMAP(n_neighbors=n_neighbors, min_dist=min_dist, n_components=2, random_state=42)
    embedding = reducer.fit_transform(features)
    
    colors = [action_mask_to_color(int(m)) for m in action_masks]
    
    fig, ax = plt.subplots(figsize=(12, 10))
    
    ax.scatter(embedding[:, 0], embedding[:, 1], c=colors, alpha=0.5, s=10)
    
    legend_elements = [
        plt.Line2D([0], [0], marker='o', color='w', markerfacecolor='red', markersize=10, label='Attack'),
        plt.Line2D([0], [0], marker='o', color='w', markerfacecolor='green', markersize=10, label='Eat'),
        plt.Line2D([0], [0], marker='o', color='w', markerfacecolor='blue', markersize=10, label='Jump'),
        plt.Line2D([0], [0], marker='o', color='w', markerfacecolor='gray', markersize=10, label='Idle'),
    ]
    ax.legend(handles=legend_elements, loc='upper right')
    
    ax.set_xlabel('UMAP Dimension 1')
    ax.set_ylabel('UMAP Dimension 2')
    ax.set_title('Brain Activation Manifold (colored by action)')
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Saved: {output_path}")


def plot_manifold_tsne(features: np.ndarray,
                       action_masks: np.ndarray,
                       output_path: Path) -> None:
    """Apply t-SNE and plot 2D manifold (fallback if UMAP unavailable)."""
    try:
        from sklearn.manifold import TSNE
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError as e:
        print(f"Missing dependency: {e}")
        return
    
    valid_mask = np.any(features != 0, axis=1)
    if np.sum(valid_mask) < 10:
        print("Not enough non-zero samples for t-SNE.")
        return
    
    features = features[valid_mask]
    action_masks = action_masks[valid_mask]
    
    if len(features) > 2000:
        indices = np.random.choice(len(features), 2000, replace=False)
        features = features[indices]
        action_masks = action_masks[indices]
    
    print(f"Running t-SNE on {len(features)} samples...")
    tsne = TSNE(n_components=2, random_state=42, perplexity=min(30, len(features) - 1))
    embedding = tsne.fit_transform(features)
    
    colors = [action_mask_to_color(int(m)) for m in action_masks]
    
    fig, ax = plt.subplots(figsize=(12, 10))
    ax.scatter(embedding[:, 0], embedding[:, 1], c=colors, alpha=0.5, s=10)
    ax.set_title('Brain Activation Manifold (t-SNE)')
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(description='Visualize brain activation manifolds.')
    parser.add_argument('--input', '-i', type=str, default='telemetry/brain_activations.csv',
                        help='Path to brain_activations.csv')
    parser.add_argument('--output', '-o', type=str, default='manifold.png',
                        help='Output file path')
    parser.add_argument('--method', type=str, choices=['umap', 'tsne'], default='umap',
                        help='Dimensionality reduction method')
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_path = Path(args.output)
    
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1
    
    print(f"Loading activations from: {input_path}")
    time, entity_ids, action_masks, features = load_activations(input_path)
    print(f"Loaded {len(features)} samples with {features.shape[1]} features")
    
    if args.method == 'umap':
        plot_manifold_umap(features, action_masks, output_path)
    else:
        plot_manifold_tsne(features, action_masks, output_path)
    
    print("Manifold visualization complete.")
    return 0


if __name__ == '__main__':
    exit(main())
