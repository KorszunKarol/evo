#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BASE_BRANCH="${1:-master}"

if ! git rev-parse --verify "$BASE_BRANCH" >/dev/null 2>&1; then
  echo "Base branch '$BASE_BRANCH' not found."
  exit 1
fi

echo "== Branch Audit (base: $BASE_BRANCH) =="
echo

printf "%-45s %-12s %-12s %s\n" "branch" "behind" "ahead" "head"
printf "%-45s %-12s %-12s %s\n" "------" "------" "-----" "----"

while IFS= read -r branch; do
  if [[ "$branch" == "$BASE_BRANCH" ]]; then
    continue
  fi
  read -r behind ahead <<<"$(git rev-list --left-right --count "$BASE_BRANCH...$branch")"
  head="$(git rev-parse --short "$branch")"
  printf "%-45s %-12s %-12s %s\n" "$branch" "$behind" "$ahead" "$head"
done < <(git for-each-ref --format='%(refname:short)' refs/heads | sort)

echo
echo "Suggested cleanup policy:"
echo "- delete branches with ahead=0 and behind=0 (exact duplicates)"
echo "- rebase branches with behind>0 and ahead>0"
echo "- archive stale branches via tag before delete"
