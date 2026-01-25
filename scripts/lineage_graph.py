#!/usr/bin/env python3
"""
Phylogeny Lineage Graph Generator for Evolution Simulation.

Parses events.jsonl for SPAWN events and reconstructs the family tree.
Exports Graphviz DOT files and can render directly if graphviz is installed.

Usage:
    python3 lineage_graph.py --input telemetry/events.jsonl --output lineage.dot
"""

import argparse
import json
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Tuple, Set


def parse_spawn_events(filepath: Path) -> List[Dict]:
    """Parse SPAWN events from events.jsonl."""
    spawns = []
    
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
                if event.get('type') == 'SPAWN':
                    spawns.append({
                        'id': event.get('id', 0),
                        'parent': event.get('parent', 0),
                        'time': event.get('t', 0.0),
                        'x': event.get('x', 0.0),
                        'z': event.get('z', 0.0),
                    })
            except json.JSONDecodeError:
                continue
    
    return spawns


def build_lineage_tree(spawns: List[Dict]) -> Dict[int, List[int]]:
    """Build parent -> children mapping."""
    tree = defaultdict(list)
    
    for spawn in spawns:
        parent_id = spawn['parent']
        child_id = spawn['id']
        if parent_id != 0:
            tree[parent_id].append(child_id)
    
    return tree


def find_root_nodes(spawns: List[Dict], tree: Dict[int, List[int]]) -> Set[int]:
    """Find nodes that have no incoming edges (root ancestors)."""
    all_nodes = set()
    children = set()
    
    for spawn in spawns:
        all_nodes.add(spawn['id'])
        if spawn['parent'] != 0:
            all_nodes.add(spawn['parent'])
            children.add(spawn['id'])
    
    parents_with_children = set(tree.keys())
    roots = parents_with_children - children
    
    orphans = {s['id'] for s in spawns if s['parent'] == 0}
    roots.update(orphans)
    
    return roots


def compute_dynasty_sizes(tree: Dict[int, List[int]], roots: Set[int]) -> Dict[int, int]:
    """Compute total offspring count for each dynasty."""
    def count_descendants(node: int) -> int:
        children = tree.get(node, [])
        return len(children) + sum(count_descendants(c) for c in children)
    
    return {root: count_descendants(root) for root in roots}


def export_dot(tree: Dict[int, List[int]], 
               roots: Set[int],
               dynasty_sizes: Dict[int, int],
               output_path: Path,
               max_nodes: int = 500) -> None:
    """Export lineage tree to Graphviz DOT format."""
    
    sorted_roots = sorted(roots, key=lambda r: dynasty_sizes.get(r, 0), reverse=True)
    
    included_nodes = set()
    queue = []
    
    for root in sorted_roots:
        if len(included_nodes) >= max_nodes:
            break
        queue.append(root)
        while queue and len(included_nodes) < max_nodes:
            node = queue.pop(0)
            if node in included_nodes:
                continue
            included_nodes.add(node)
            queue.extend(tree.get(node, []))
    
    with open(output_path, 'w') as f:
        f.write('digraph Lineage {\n')
        f.write('    rankdir=TB;\n')
        f.write('    node [shape=circle, style=filled, fillcolor=lightblue];\n')
        f.write('    edge [color=gray];\n')
        f.write('\n')
        
        for root in sorted_roots:
            if root in included_nodes:
                size = dynasty_sizes.get(root, 0)
                f.write(f'    {root} [fillcolor=gold, label="{root}\\n({size})"];\n')
        
        f.write('\n')
        
        for parent, children in tree.items():
            if parent not in included_nodes:
                continue
            for child in children:
                if child in included_nodes:
                    f.write(f'    {parent} -> {child};\n')
        
        f.write('}\n')
    
    print(f"Saved DOT file: {output_path}")
    print(f"Included {len(included_nodes)} nodes (max: {max_nodes})")


def render_graph(dot_path: Path, output_path: Path, fmt: str = 'png') -> None:
    """Render DOT file to image using graphviz."""
    try:
        import subprocess
        result = subprocess.run(
            ['dot', f'-T{fmt}', str(dot_path), '-o', str(output_path)],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            print(f"Rendered graph: {output_path}")
        else:
            print(f"Graphviz error: {result.stderr}")
    except FileNotFoundError:
        print("Graphviz not installed. Install with: sudo apt install graphviz")


def generate_dynasty_report(dynasty_sizes: Dict[int, int], output_path: Path) -> None:
    """Generate a text report of dominant dynasties."""
    sorted_dynasties = sorted(dynasty_sizes.items(), key=lambda x: x[1], reverse=True)
    
    with open(output_path, 'w') as f:
        f.write("Dominant Dynasties Report\n")
        f.write("=" * 40 + "\n\n")
        f.write(f"Total dynasties: {len(sorted_dynasties)}\n\n")
        
        f.write("Top 20 Dynasties:\n")
        f.write("-" * 40 + "\n")
        f.write(f"{'Rank':<6}{'Root ID':<12}{'Offspring':<12}\n")
        f.write("-" * 40 + "\n")
        
        for i, (root, size) in enumerate(sorted_dynasties[:20], 1):
            f.write(f"{i:<6}{root:<12}{size:<12}\n")
        
        total_offspring = sum(dynasty_sizes.values())
        f.write(f"\nTotal tracked offspring: {total_offspring}\n")
    
    print(f"Saved dynasty report: {output_path}")


def main():
    parser = argparse.ArgumentParser(description='Generate lineage/phylogeny graphs from telemetry.')
    parser.add_argument('--input', '-i', type=str, default='telemetry/events.jsonl',
                        help='Path to events.jsonl file')
    parser.add_argument('--output', '-o', type=str, default='lineage',
                        help='Output file prefix (without extension)')
    parser.add_argument('--max-nodes', type=int, default=500,
                        help='Maximum nodes to include in graph')
    parser.add_argument('--render', action='store_true',
                        help='Render to PNG (requires graphviz)')
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_dir = input_path.parent
    
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1
    
    print(f"Parsing spawn events from: {input_path}")
    spawns = parse_spawn_events(input_path)
    print(f"Found {len(spawns)} spawn events")
    
    if not spawns:
        print("No spawn events found. Exiting.")
        return 0
    
    tree = build_lineage_tree(spawns)
    roots = find_root_nodes(spawns, tree)
    dynasty_sizes = compute_dynasty_sizes(tree, roots)
    
    print(f"Found {len(roots)} dynasty roots")
    
    dot_path = output_dir / f'{args.output}.dot'
    export_dot(tree, roots, dynasty_sizes, dot_path, args.max_nodes)
    
    report_path = output_dir / f'{args.output}_report.txt'
    generate_dynasty_report(dynasty_sizes, report_path)
    
    if args.render:
        png_path = output_dir / f'{args.output}.png'
        render_graph(dot_path, png_path)
    
    print("Lineage graph generation complete.")
    return 0


if __name__ == '__main__':
    exit(main())
