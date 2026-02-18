#!/usr/bin/env python3
from __future__ import annotations

import argparse
import html
import sys
from pathlib import Path

import pandas as pd

if __package__ is None or __package__ == "":
    sys.path.append(str(Path(__file__).resolve().parents[1]))

from telemetry.common import (
    ensure_out_dir,
    load_metrics,
    load_species_rollups,
    maybe_load_events,
    save_summary_json,
)
from telemetry.energy import analyze_energy
from telemetry.population import analyze_population
from telemetry.species import analyze_species


def _render_section(title: str, charts: list[str], chart_root: str) -> str:
    items = []
    for chart in charts:
        safe = html.escape(chart)
        src = html.escape(f"{chart_root}/{chart}")
        items.append(
            f"<div class='card'><h3>{safe}</h3><img src='{src}' alt='{safe}' /></div>"
        )
    return f"<section><h2>{html.escape(title)}</h2>{''.join(items)}</section>"


def _render_summary_block(title: str, summary: dict) -> str:
    rows = []
    for key, value in sorted(summary.items()):
        rows.append(f"<tr><td>{html.escape(str(key))}</td><td>{html.escape(str(value))}</td></tr>")
    return (
        f"<section><h2>{html.escape(title)}</h2>"
        "<table><thead><tr><th>Metric</th><th>Value</th></tr></thead>"
        f"<tbody>{''.join(rows)}</tbody></table></section>"
    )


def _write_html(path: Path, payload: dict) -> None:
    html_doc = f"""<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <title>Simulation Telemetry Report</title>
  <style>
    body {{ font-family: Helvetica, Arial, sans-serif; margin: 24px; background: #f8fafc; color: #0f172a; }}
    h1, h2 {{ margin-bottom: 8px; }}
    .meta {{ margin-bottom: 18px; }}
    .grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(460px, 1fr)); gap: 12px; }}
    .card {{ border: 1px solid #cbd5e1; background: #fff; border-radius: 6px; padding: 10px; }}
    .card img {{ width: 100%; height: auto; border: 1px solid #e2e8f0; }}
    table {{ border-collapse: collapse; width: min(900px, 100%); background: #fff; border: 1px solid #cbd5e1; }}
    th, td {{ border: 1px solid #cbd5e1; padding: 6px 8px; text-align: left; }}
  </style>
</head>
<body>
  <h1>Simulation Telemetry Report</h1>
  <div class='meta'>
    <p><strong>Run ID:</strong> {html.escape(str(payload['run_id']))}</p>
    <p><strong>Telemetry Dir:</strong> {html.escape(str(payload['telemetry_dir']))}</p>
    <p><strong>Total Anomalies:</strong> {payload['total_anomalies']}</p>
  </div>
  {_render_summary_block('Population Summary', payload['population_summary'])}
  {_render_summary_block('Species Summary', payload['species_summary'])}
  {_render_summary_block('Energy Summary', payload['energy_summary'])}
  <section>
    <h2>Population Charts</h2>
    <div class='grid'>
      {''.join([f"<div class='card'><img src='charts/population/{html.escape(c)}' alt='{html.escape(c)}'><div>{html.escape(c)}</div></div>" for c in payload['population_charts']])}
    </div>
  </section>
  <section>
    <h2>Species Charts</h2>
    <div class='grid'>
      {''.join([f"<div class='card'><img src='charts/species/{html.escape(c)}' alt='{html.escape(c)}'><div>{html.escape(c)}</div></div>" for c in payload['species_charts']])}
    </div>
  </section>
  <section>
    <h2>Energy Charts</h2>
    <div class='grid'>
      {''.join([f"<div class='card'><img src='charts/energy/{html.escape(c)}' alt='{html.escape(c)}'><div>{html.escape(c)}</div></div>" for c in payload['energy_charts']])}
    </div>
  </section>
  <section>
    <h2>Anomaly Files</h2>
    <ul>
      <li><a href='anomalies.csv'>anomalies.csv</a></li>
      <li><a href='summary.json'>summary.json</a></li>
    </ul>
  </section>
</body>
</html>
"""
    path.write_text(html_doc, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate full simulation telemetry report.")
    parser.add_argument("--telemetry-dir", type=Path, default=Path("output/telemetry"))
    parser.add_argument("--out-dir", type=Path, default=Path("output/reports/latest"))
    parser.add_argument("--run-id", type=str, default=None)
    parser.add_argument("--use-events", action="store_true")
    parser.add_argument("--rolling-window-sec", type=float, default=20.0)
    args = parser.parse_args()

    charts_root = args.out_dir / "charts"
    pop_dir = charts_root / "population"
    species_dir = charts_root / "species"
    energy_dir = charts_root / "energy"
    for path in [args.out_dir, charts_root, pop_dir, species_dir, energy_dir]:
        ensure_out_dir(path)

    metrics = load_metrics(args.telemetry_dir, run_id=args.run_id)
    species = load_species_rollups(args.telemetry_dir, run_id=args.run_id)
    events = pd.DataFrame()
    if args.use_events:
        events = maybe_load_events(args.telemetry_dir, run_id=args.run_id)

    pop_charts, pop_summary, pop_anoms = analyze_population(metrics, pop_dir, args.rolling_window_sec)
    species_charts, species_summary, species_anoms = analyze_species(species, species_dir)
    energy_charts, energy_summary, energy_anoms = analyze_energy(
        metrics,
        energy_dir,
        events=events,
        rolling_window_sec=args.rolling_window_sec,
    )

    all_anoms = pd.concat([pop_anoms, species_anoms, energy_anoms], ignore_index=True)
    if not all_anoms.empty:
        all_anoms = all_anoms.sort_values("sim_time")
    all_anoms.to_csv(args.out_dir / "anomalies.csv", index=False)

    inferred_run_id = args.run_id if args.run_id else str(metrics["run_id"].iloc[0])
    summary_payload = {
        "run_id": inferred_run_id,
        "telemetry_dir": str(args.telemetry_dir),
        "population_summary": pop_summary,
        "species_summary": species_summary,
        "energy_summary": energy_summary,
        "population_charts": pop_charts,
        "species_charts": species_charts,
        "energy_charts": energy_charts,
        "total_anomalies": int(len(all_anoms)),
    }

    save_summary_json(args.out_dir / "summary.json", summary_payload)
    _write_html(args.out_dir / "index.html", summary_payload)

    print(f"Report generated in {args.out_dir}")
    print(f"Charts: population={len(pop_charts)}, species={len(species_charts)}, energy={len(energy_charts)}")
    print(f"Anomalies: {len(all_anoms)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
