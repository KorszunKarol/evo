#!/usr/bin/env python3
"""
Ecosystem Energy Flux Visualization (Sankey Diagram).

Generates Sankey diagrams showing energy flow through trophic levels.

Usage:
    python3 energy_flux.py --input telemetry/metrics.csv --output energy_sankey.html
"""

import argparse
from pathlib import Path
from typing import Dict, List


def load_metrics_csv(filepath: Path) -> Dict[str, List[float]]:
    """Load metrics.csv into a dictionary."""
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


def generate_sankey_plotly(data: Dict[str, List[float]], output_path: Path) -> None:
    """Generate Sankey diagram using Plotly."""
    try:
        import plotly.graph_objects as go
    except ImportError:
        print("plotly not installed. Install with: pip install plotly")
        return
    
    plant_biomass = sum(data.get('biomass_producers', []))
    consumer_biomass = sum(data.get('biomass_consumers', []))
    corpse_biomass = sum(data.get('biomass_corpses', []))
    soil_biomass = sum(data.get('biomass_soil', []))
    
    hunting_energy = sum(data.get('energy_hunting', []))
    scavenging_energy = sum(data.get('energy_scavenging', []))
    
    nodes = [
        'Sun (Input)',         # 0
        'Plants',              # 1
        'Herbivores',          # 2
        'Carnivores',          # 3
        'Corpses',             # 4
        'Soil',                # 5
        'Metabolism (Lost)',   # 6
    ]
    
    links = {
        'source': [],
        'target': [],
        'value': [],
        'label': [],
    }
    
    def add_link(src, tgt, val, label=''):
        if val > 0:
            links['source'].append(src)
            links['target'].append(tgt)
            links['value'].append(val)
            links['label'].append(label)
    
    add_link(0, 1, plant_biomass / 10, 'Photosynthesis')
    add_link(1, 2, consumer_biomass * 0.6, 'Grazing')
    add_link(2, 3, hunting_energy if hunting_energy > 0 else consumer_biomass * 0.1, 'Predation')
    add_link(3, 4, corpse_biomass * 0.5, 'Death')
    add_link(2, 4, corpse_biomass * 0.5, 'Death')
    add_link(4, 3, scavenging_energy if scavenging_energy > 0 else corpse_biomass * 0.3, 'Scavenging')
    add_link(4, 5, soil_biomass * 0.1, 'Decomposition')
    add_link(5, 1, soil_biomass * 0.1, 'Nutrient Uptake')
    add_link(2, 6, consumer_biomass * 0.2, 'Herbivore Metabolism')
    add_link(3, 6, consumer_biomass * 0.1, 'Carnivore Metabolism')
    
    fig = go.Figure(data=[go.Sankey(
        node=dict(
            pad=15,
            thickness=20,
            line=dict(color='black', width=0.5),
            label=nodes,
            color=['yellow', 'green', 'lightblue', 'red', 'brown', 'darkgray', 'gray']
        ),
        link=dict(
            source=links['source'],
            target=links['target'],
            value=links['value'],
            label=links['label'],
        )
    )])
    
    fig.update_layout(
        title_text='Ecosystem Energy Flux',
        font_size=12,
    )
    
    fig.write_html(str(output_path))
    print(f"Saved: {output_path}")


def generate_flow_summary(data: Dict[str, List[float]], output_path: Path) -> None:
    """Generate text summary of energy flows."""
    
    summary_lines = [
        "Ecosystem Energy Flux Summary",
        "=" * 40,
        "",
    ]
    
    if 'biomass_producers' in data:
        avg_plant = sum(data['biomass_producers']) / max(len(data['biomass_producers']), 1)
        summary_lines.append(f"Average Plant Biomass: {avg_plant:.2f}")
    
    if 'biomass_consumers' in data:
        avg_consumer = sum(data['biomass_consumers']) / max(len(data['biomass_consumers']), 1)
        summary_lines.append(f"Average Consumer Biomass: {avg_consumer:.2f}")
    
    if 'energy_hunting' in data:
        total_hunting = sum(data['energy_hunting'])
        summary_lines.append(f"Total Hunting Energy: {total_hunting:.2f}")
    
    if 'energy_scavenging' in data:
        total_scav = sum(data['energy_scavenging'])
        summary_lines.append(f"Total Scavenging Energy: {total_scav:.2f}")
    
    if 'deaths_starvation' in data:
        starvation = int(data['deaths_starvation'][-1]) if data['deaths_starvation'] else 0
        summary_lines.append(f"Total Starvation Deaths: {starvation}")
    
    if 'deaths_predation' in data:
        predation = int(data['deaths_predation'][-1]) if data['deaths_predation'] else 0
        summary_lines.append(f"Total Predation Deaths: {predation}")
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(summary_lines))
    
    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(description='Generate energy flux Sankey diagram.')
    parser.add_argument('--input', '-i', type=str, default='telemetry/metrics.csv',
                        help='Path to metrics.csv file')
    parser.add_argument('--output', '-o', type=str, default='energy_sankey.html',
                        help='Output file path')
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_path = Path(args.output)
    
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1
    
    print(f"Loading metrics from: {input_path}")
    data = load_metrics_csv(input_path)
    
    print(f"Loaded {len(data.get('time', []))} samples")
    
    generate_sankey_plotly(data, output_path)
    
    summary_path = output_path.with_suffix('.txt')
    generate_flow_summary(data, summary_path)
    
    print("Energy flux visualization complete.")
    return 0


if __name__ == '__main__':
    exit(main())
