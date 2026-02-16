#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "== Project Health =="
echo "date: $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
echo "branch: $(git rev-parse --abbrev-ref HEAD)"
echo "commit: $(git rev-parse --short HEAD)"
echo

echo "== Working Tree =="
git status --short --branch
echo

echo "== Branch Divergence =="
if git rev-parse --verify origin/master >/dev/null 2>&1; then
  echo "origin/master...master: $(git rev-list --left-right --count origin/master...master | xargs)"
else
  echo "origin/master...master: unavailable (no origin/master)"
fi

for b in $(git for-each-ref --format='%(refname:short)' refs/heads); do
  if [[ "$b" == "master" ]]; then
    continue
  fi
  echo "master...$b: $(git rev-list --left-right --count master...$b | xargs)"
done
echo

echo "== Stashes =="
git stash list || true
echo

echo "== Build + Tests =="
if [[ -d build ]]; then
  echo "[build] cmake --build build"
  if cmake --build build -j"$(( $(nproc) / 2 ))" >/tmp/project_health_build.log 2>&1; then
    echo "build: PASS"
  else
    echo "build: FAIL"
    tail -n 40 /tmp/project_health_build.log
    exit 1
  fi

  echo "[test] ctest --test-dir build --output-on-failure"
  if ctest --test-dir build --output-on-failure >/tmp/project_health_test.log 2>&1; then
    echo "tests: PASS"
  else
    echo "tests: FAIL"
    tail -n 80 /tmp/project_health_test.log
    exit 1
  fi
else
  echo "build directory not found; skipping build/test checks."
fi
