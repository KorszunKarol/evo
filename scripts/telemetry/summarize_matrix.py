#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Aggregate per-preset invariant summaries.")
    parser.add_argument("--matrix-dir", type=Path, required=True)
    parser.add_argument("--out-json", type=Path, required=True)
    args = parser.parse_args()

    presets = []
    for child in sorted(args.matrix_dir.iterdir()):
        if not child.is_dir():
            continue
        inv = child / "invariants.json"
        if not inv.exists():
            presets.append({
                "preset": child.name,
                "passed": False,
                "error": "missing invariants.json",
            })
            continue
        try:
            payload = json.loads(inv.read_text(encoding="utf-8"))
        except Exception as exc:
            presets.append({
                "preset": child.name,
                "passed": False,
                "error": f"invalid json: {exc}",
            })
            continue
        presets.append(
            {
                "preset": child.name,
                "passed": bool(payload.get("passed", False)),
                "summary": payload.get("summary", {}),
            }
        )

    failed = [p["preset"] for p in presets if not p.get("passed", False)]
    matrix_payload = {
        "matrix_dir": str(args.matrix_dir),
        "passed": len(failed) == 0,
        "failed_presets": failed,
        "preset_count": len(presets),
        "results": presets,
    }

    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    args.out_json.write_text(json.dumps(matrix_payload, indent=2), encoding="utf-8")

    if failed:
        print("[matrix-summary] FAIL presets=" + ",".join(failed))
        return 2
    print("[matrix-summary] PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
