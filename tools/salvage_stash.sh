#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [[ "${1:-}" != "--apply" ]]; then
  echo "Dry run mode."
  echo "Usage: ./tools/salvage_stash.sh --apply"
  echo "This will create salvage branches from each stash using temporary worktrees."
  exit 0
fi

if [[ -z "$(git stash list)" ]]; then
  echo "No stashes found."
  exit 0
fi

tmp_root="/tmp/evo-stash-salvage"
mkdir -p "$tmp_root"

while IFS= read -r line; do
  stash_ref="${line%%:*}"
  safe_name="$(printf '%s' "$stash_ref" | sed -E 's/[^a-zA-Z0-9._-]+/_/g')"
  branch_name="salvage/${safe_name}"
  wt_dir="$tmp_root/$safe_name"

  echo "Processing $stash_ref -> $branch_name"

  # Derive stash base commit (2nd parent of stash commit)
  stash_commit="$(git rev-parse "$stash_ref")"
  base_commit="$(git rev-parse "${stash_commit}^2" 2>/dev/null || true)"
  if [[ -z "$base_commit" ]]; then
    # Fallback to 1st parent for older stash shapes.
    base_commit="$(git rev-parse "${stash_commit}^1")"
  fi

  git worktree add -b "$branch_name" "$wt_dir" "$base_commit"
  if git stash show -p "$stash_ref" | git -C "$wt_dir" apply -3; then
    git -C "$wt_dir" add -A
    git -C "$wt_dir" commit -m "salvage $stash_ref"
    git push -u origin "$branch_name"
    echo "Created and pushed $branch_name"
  else
    echo "Failed applying $stash_ref cleanly in $branch_name"
    echo "Inspect worktree: $wt_dir"
  fi

  git worktree remove "$wt_dir" --force
done < <(git stash list)

echo "Done."
