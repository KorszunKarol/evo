#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 TRACE_FILE [OUT_DIR] [FILTER]"
  echo "  TRACE_FILE: Path to .tracy capture"
  echo "  OUT_DIR   : Output directory (default: /home/karolito/evolution/output/tracy)"
  echo "  FILTER    : Optional zone name filter (csvexport -f)"
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

trace_file="$1"
out_dir="${2:-/home/karolito/evolution/output/tracy}"
filter="${3:-}"

csvexport="${CSVEXPORT:-/home/karolito/evolution/build/_deps/tracy-src/csvexport/build/tracy-csvexport}"

if [[ ! -x "$csvexport" ]]; then
  echo "csvexport not found or not executable: $csvexport"
  exit 1
fi

if [[ ! -f "$trace_file" ]]; then
  echo "Trace file not found: $trace_file"
  exit 1
fi

mkdir -p "$out_dir"

base_name="$(basename "$trace_file")"
base_name="${base_name%.*}"

args=()
if [[ -n "$filter" ]]; then
  args+=("-f" "$filter")
fi

"$csvexport" "${args[@]}" "$trace_file" > "$out_dir/${base_name}.summary.csv"
"$csvexport" -e "${args[@]}" "$trace_file" > "$out_dir/${base_name}.summary.self.csv"
"$csvexport" -u "${args[@]}" "$trace_file" > "$out_dir/${base_name}.unwrap.csv"
"$csvexport" -u -e "${args[@]}" "$trace_file" > "$out_dir/${base_name}.unwrap.self.csv"
"$csvexport" -u -p "${args[@]}" "$trace_file" > "$out_dir/${base_name}.unwrap.plot.csv"

echo "Wrote CSV exports to $out_dir"
