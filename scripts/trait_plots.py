#!/usr/bin/env python3
"""
Trait Evolution Plots for Evolution Simulation.

Parses traits.csv and generates visualizations showing how
phenotypic traits evolve over simulation time.

Usage:
    python3 trait_plots.py --input telemetry/traits.csv --output traits_evolution.png
"""

import argparse
from pathlib import Path
from typing import Dict, List

import numpy as np


def load_traits_csv(filepath: Path) -> Dict[str, List[float]]:
    """Load traits.csv into a dictionary of lists."""
    data: Dict[str, List[float]] = {}
    
    with open(filepath, 'r') as f:
        header = f.readline().strip().split(',')
        for col in header:
            data[col] = []
        
        for line in f:
            values = line.strip().split(',')
            for i, col in enumerate(header):
                try:
                    data[col].append(float(values[i]))
                except (ValueError, IndexError):
                    data[col].append(0.0)
    
    return data


def plot_trait_evolution(data: Dict[str, List[float]], 
                         output_path: Path,
                         trait_groups: List[tuple]) -> None:
    """Plot trait evolution over time."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed. Install with: pip install matplotlib")
        return
    
    time = data.get('time', [])
    if not time:
        print("No time data found.")
        return
    
    n_groups = len(trait_groups)
    fig, axes = plt.subplots(n_groups, 1, figsize=(12, 4 * n_groups), sharex=True)
    
    if n_groups == 1:
        axes = [axes]
    
    for ax, (title, mean_col, var_col, min_col, max_col) in zip(axes, trait_groups):
        mean_vals = np.array(data.get(mean_col, []))
        var_vals = np.array(data.get(var_col, []))
        min_vals = np.array(data.get(min_col, []))
        max_vals = np.array(data.get(max_col, []))
        
        if len(mean_vals) == 0:
            continue
        
        std_vals = np.sqrt(var_vals)
        
        ax.fill_between(time, min_vals, max_vals, alpha=0.2, label='Range')
        ax.fill_between(time, mean_vals - std_vals, mean_vals + std_vals, 
                        alpha=0.4, label='±1 std')
        ax.plot(time, mean_vals, 'b-', linewidth=2, label='Mean')
        
        ax.set_ylabel(title)
        ax.legend(loc='upper right')
        ax.grid(True, alpha=0.3)
    
    axes[-1].set_xlabel('Simulation Time (s)')
    
    fig.suptitle('Trait Evolution Over Time', fontsize=14)
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Saved: {output_path}")


def plot_population(data: Dict[str, List[float]], output_path: Path) -> None:
    """Plot population over time."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        return
    
    time = data.get('time', [])
    population = data.get('population', [])
    
    if not time or not population:
        return
    
    fig, ax = plt.subplots(figsize=(12, 4))
    ax.plot(time, population, 'g-', linewidth=2)
    ax.set_xlabel('Simulation Time (s)')
    ax.set_ylabel('Population')
    ax.set_title('Population Over Time')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Saved: {output_path}")


def plot_correlation_matrix(data: Dict[str, List[float]], output_path: Path) -> None:
    """Plot correlation matrix between traits (only those with variance)."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        return
    
    trait_cols = [
        'vision_range_mean', 'vision_fov_mean', 
        'basal_rate_mean', 'max_energy_mean'
    ]
    
    # Filter to traits that exist and have non-zero variance
    available = []
    for c in trait_cols:
        if c in data and len(data[c]) > 0:
            vals = np.array(data[c])
            if np.var(vals) > 1e-10:  # Has meaningful variance
                available.append(c)
    
    if len(available) < 2:
        print("Skipping correlation matrix: fewer than 2 traits with variance")
        return
    
    matrix_data = np.array([data[c] for c in available]).T
    
    if matrix_data.shape[0] < 2:
        return
    
    corr = np.corrcoef(matrix_data.T)
    
    fig, ax = plt.subplots(figsize=(8, 6))
    im = ax.imshow(corr, cmap='RdBu_r', vmin=-1, vmax=1)
    
    ax.set_xticks(range(len(available)))
    ax.set_yticks(range(len(available)))
    
    short_names = [c.replace('_mean', '').replace('_', ' ').title() for c in available]
    ax.set_xticklabels(short_names, rotation=45, ha='right')
    ax.set_yticklabels(short_names)
    
    for i in range(len(available)):
        for j in range(len(available)):
            ax.text(j, i, f'{corr[i, j]:.2f}', ha='center', va='center', fontsize=10)
    
    plt.colorbar(im, ax=ax, label='Correlation')
    ax.set_title('Trait Correlation Matrix')
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(description='Generate trait evolution plots.')
    parser.add_argument('--input', '-i', type=str, default='telemetry/traits.csv',
                        help='Path to traits.csv file')
    parser.add_argument('--output', '-o', type=str, default='trait_evolution.png',
                        help='Output file path')
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_base = Path(args.output).stem
    output_dir = Path(args.output).parent or Path('.')
    
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1
    
    print(f"Loading traits from: {input_path}")
    data = load_traits_csv(input_path)
    
    print(f"Loaded {len(data.get('time', []))} samples")
    
    trait_groups = [
        ('Vision Range (m)', 'vision_range_mean', 'vision_range_var', 
         'vision_range_min', 'vision_range_max'),
        ('Vision FOV (rad)', 'vision_fov_mean', 'vision_fov_var',
         'vision_fov_min', 'vision_fov_max'),
        ('Basal Rate (E/s)', 'basal_rate_mean', 'basal_rate_var',
         'basal_rate_min', 'basal_rate_max'),
        ('Max Energy', 'max_energy_mean', 'max_energy_var',
         'max_energy_min', 'max_energy_max'),
    ]
    
    plot_trait_evolution(data, output_dir / f'{output_base}_evolution.png', trait_groups)
    plot_population(data, output_dir / f'{output_base}_population.png')
    plot_correlation_matrix(data, output_dir / f'{output_base}_correlation.png')
    
    print("Trait visualization complete.")
    return 0


if __name__ == '__main__':
    exit(main())
