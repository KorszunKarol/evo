# Branching Playbook

This repository now uses a lightweight, PR-driven flow without CI gates.

## Protected Branches
- `master`

Protection settings:
- no required status checks
- 1 required approving review
- conversation resolution required
- no force push
- no branch deletion

## Daily Workflow
1. Start from integration branch:
   - `git checkout master`
   - `git pull --rebase`
2. Create a short-lived branch:
   - `git checkout -b fix/<topic>`
3. Implement and run local checks:
   - `cmake --build build`
   - `ctest --test-dir build --output-on-failure`
4. Open PR into `master`.
5. Merge only after review.

## Branch Debt Cleanup
- Audit branches:
  - `./tools/branch_audit.sh master`
- Audit stashes:
  - `./tools/stash_audit.sh`
- Preserve branch tips before deletion using backup tags:
  - `branch_backup_2026_02_16_*`

## Merge-Up Strategy
- Keep `master` as the single integration baseline.
- Avoid long-lived feature branches with large unresolved divergence.
