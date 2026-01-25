#!/usr/bin/env python3
"""
Spatial Analytics Heatmap Generator for Evolution Simulation.

Parses events.jsonl and generates 2D density heatmaps for:
- Death locations (death valleys)
- Feeding hotspots
- Spawn locations (population centers)

Usage:
    python3 heatmap.py --input telemetry/events.jsonl --output heatmaps/
"""

import argparse
import json
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Tuple

import numpy as np


def parse_events(filepath: Path, terrain_bounds: Tuple[float, float] = (0.0, 128.0)) -> Dict[str, List[Tuple[float, float]]]:
    """Parse events.jsonl and extract spatial coordinates by event type.
    
    Filters out coordinates outside terrain bounds to exclude garbage data.
    """
    events_by_type: Dict[str, List[Tuple[float, float]]] = defaultdict(list)
    
    min_bound, max_bound = terrain_bounds
    
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
                event_type = event.get('type', 'UNKNOWN')
                x = event.get('x', 0.0)
                z = event.get('z', 0.0)
                # Filter out coordinates outside terrain bounds
                if min_bound <= x <= max_bound and min_bound <= z <= max_bound:
                    events_by_type[event_type].append((x, z))
            except json.JSONDecodeError:
                continue
    
    return events_by_type


def compute_kde_density(points: List[Tuple[float, float]], 
                        grid_size: int = 100,
                        bandwidth: float = 2.0) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Compute 2D Kernel Density Estimation using Gaussian kernels."""
    if not points:
        return np.zeros((grid_size, grid_size)), np.array([]), np.array([])
    
    x_coords = np.array([p[0] for p in points])
    z_coords = np.array([p[1] for p in points])
    
    x_min, x_max = x_coords.min() - 5, x_coords.max() + 5
    z_min, z_max = z_coords.min() - 5, z_coords.max() + 5
    
    x_grid = np.linspace(x_min, x_max, grid_size)
    z_grid = np.linspace(z_min, z_max, grid_size)
    xx, zz = np.meshgrid(x_grid, z_grid)
    
    density = np.zeros_like(xx)
    for px, pz in points:
        density += np.exp(-((xx - px)**2 + (zz - pz)**2) / (2 * bandwidth**2))
    
    density /= (2 * np.pi * bandwidth**2 * len(points))
    
    return density, x_grid, z_grid


def generate_heatmap(density: np.ndarray, 
                     x_grid: np.ndarray, 
                     z_grid: np.ndarray,
                     title: str,
                     output_path: Path,
                     cmap: str = 'hot') -> None:
    """Generate and save a heatmap image."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed. Install with: pip install matplotlib")
        return
    
    fig, ax = plt.subplots(figsize=(10, 10))
    
    im = ax.imshow(density, extent=[x_grid.min(), x_grid.max(), z_grid.min(), z_grid.max()],
                   origin='lower', cmap=cmap, aspect='equal')
    
    ax.set_xlabel('X Position (m)')
    ax.set_ylabel('Z Position (m)')
    ax.set_title(title)
    
    cbar = plt.colorbar(im, ax=ax, shrink=0.8)
    cbar.set_label('Density')
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"Saved: {output_path}")


def generate_animated_heatmap(events_path: Path, output_path: Path, 
                              event_type: str = 'DEATH',
                              window_size: float = 10.0) -> None:
    """Generate an animated heatmap showing temporal evolution."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        from matplotlib.animation import FuncAnimation
    except ImportError:
        print("matplotlib not installed for animation.")
        return
    
    events_with_time = []
    with open(events_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
                if event.get('type') == event_type:
                    t = event.get('t', 0.0)
                    x = event.get('x', 0.0)
                    z = event.get('z', 0.0)
                    events_with_time.append((t, x, z))
            except json.JSONDecodeError:
                continue
    
    if not events_with_time:
        print(f"No {event_type} events found for animation.")
        return
    
    events_with_time.sort(key=lambda e: e[0])
    
    all_x = [e[1] for e in events_with_time]
    all_z = [e[2] for e in events_with_time]
    x_min, x_max = min(all_x) - 5, max(all_x) + 5
    z_min, z_max = min(all_z) - 5, max(all_z) + 5
    
    fig, ax = plt.subplots(figsize=(10, 10))
    
    t_max = events_with_time[-1][0]
    frames = int(t_max / window_size) + 1
    
    def update(frame):
        ax.clear()
        t_start = frame * window_size
        t_end = (frame + 1) * window_size
        
        window_points = [(x, z) for t, x, z in events_with_time if t_start <= t < t_end]
        
        if window_points:
            density, x_grid, z_grid = compute_kde_density(window_points, grid_size=50)
            ax.imshow(density, extent=[x_min, x_max, z_min, z_max],
                     origin='lower', cmap='hot', aspect='equal')
        
        ax.set_xlim(x_min, x_max)
        ax.set_ylim(z_min, z_max)
        ax.set_title(f'{event_type} Heatmap (t={t_start:.1f}-{t_end:.1f}s)')
        ax.set_xlabel('X Position (m)')
        ax.set_ylabel('Z Position (m)')
        return []
    
    anim = FuncAnimation(fig, update, frames=frames, interval=500, blit=True)
    anim.save(str(output_path), writer='pillow', fps=2)
    plt.close(fig)
    print(f"Saved animation: {output_path}")


def main():
    parser = argparse.ArgumentParser(description='Generate spatial heatmaps from telemetry events.')
    parser.add_argument('--input', '-i', type=str, default='telemetry/events.jsonl',
                        help='Path to events.jsonl file')
    parser.add_argument('--output', '-o', type=str, default='heatmaps',
                        help='Output directory for heatmaps')
    parser.add_argument('--animate', action='store_true',
                        help='Generate animated heatmap (GIF)')
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1
    
    print(f"Parsing events from: {input_path}")
    events_by_type = parse_events(input_path)
    
    for event_type, points in events_by_type.items():
        print(f"  {event_type}: {len(points)} events")
    
    heatmap_configs = [
        ('DEATH', 'hot', 'Death Density Heatmap'),
        ('FEEDING', 'YlGn', 'Feeding Hotspots'),
        ('SPAWN', 'Blues', 'Spawn Location Density'),
    ]
    
    for event_type, cmap, title in heatmap_configs:
        points = events_by_type.get(event_type, [])
        if not points:
            print(f"Skipping {event_type}: no events")
            continue
        
        density, x_grid, z_grid = compute_kde_density(points)
        output_path = output_dir / f'{event_type.lower()}_heatmap.png'
        generate_heatmap(density, x_grid, z_grid, title, output_path, cmap)
    
    if args.animate:
        for event_type in ['DEATH', 'FEEDING', 'SPAWN']:
            if events_by_type.get(event_type):
                anim_path = output_dir / f'{event_type.lower()}_animation.gif'
                generate_animated_heatmap(input_path, anim_path, event_type)
    
    print("Heatmap generation complete.")
    return 0


if __name__ == '__main__':
    exit(main())
