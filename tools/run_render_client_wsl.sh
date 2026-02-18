#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

export DISPLAY="${DISPLAY:-:0}"
export XAUTHORITY="${XAUTHORITY:-/mnt/wslg/.Xauthority}"

if [[ -d /mnt/wslg/runtime-dir ]]; then
  export XDG_RUNTIME_DIR="/mnt/wslg/runtime-dir"
else
  export XDG_RUNTIME_DIR="/tmp/runtime-${USER}"
  mkdir -p "$XDG_RUNTIME_DIR"
  chmod 700 "$XDG_RUNTIME_DIR" || true
fi

if [[ "${RENDER_SOFTWARE_GL:-0}" == "1" ]]; then
  export LIBGL_ALWAYS_SOFTWARE=1
fi

echo "[render-wsl] DISPLAY=$DISPLAY"
echo "[render-wsl] XAUTHORITY=$XAUTHORITY"
echo "[render-wsl] XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"

echo "[render-wsl] building render_client..."
cmake --build build --target render_client -j$(( $(nproc) / 2 ))

echo "[render-wsl] launching render_client"
exec ./build/bin/render_client
