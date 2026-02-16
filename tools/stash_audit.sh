#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "== Stash Audit =="

if [[ -z "$(git stash list)" ]]; then
  echo "No stashes found."
  exit 0
fi

git stash list | nl -ba
echo

echo "== Stash Contents Summary =="
while IFS= read -r line; do
  stash_ref="${line%%:*}"
  echo "--- $stash_ref ---"
  git stash show --name-status "$stash_ref" || true
  echo
done < <(git stash list)

echo "Suggested follow-up:"
echo "1) For each stash: create branch and apply"
echo "   git stash branch salvage/<name> stash@{N}"
echo "2) Validate build/tests"
echo "3) Commit or drop stash if obsolete"
