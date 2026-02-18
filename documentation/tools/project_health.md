# Project Health Report Template

Use `tools/project_health.sh` to generate current values, then paste summary here for tracked checkpoints.
Use `tools/branch_audit.sh` and `tools/stash_audit.sh` for branch/stash debt audits.
Use `tools/salvage_stash.sh --apply` to materialize stashes into salvage branches safely.

## Checkpoint
- Date:
- Branch:
- Commit:

## Working Tree
- Dirty tracked files:
- Untracked files:

## Divergence
- `master` vs `origin/master`:
- Integration branch vs `master`:
- Active feature branches behind/ahead:

## Test Status
- Build command result:
- Full test run result:
- Failing tests (if any):
- Windows build command result (optional):
- Windows test run result (optional):

## Risks
- Crash risk:
- Determinism risk:
- Process risk (branching/CI/hygiene):
