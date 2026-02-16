# Contributing

## Branching
- Use short-lived branches from the integration branch (`feature/tracy-profiler` during stabilization).
- Branch names:
  - `fix/<topic>`
  - `refactor/<topic>`
  - `chore/<topic>`
- Rebase frequently to avoid long-lived divergence.

## Pull Requests
- Keep PRs focused and small.
- Include a short "what changed" and "how validated" section in PR description.
- Do not mix feature work with cleanup-only changes.
- Do not commit generated runtime artifacts (`output/`, `*.tracy`, local logs).

## Local Validation (Recommended)
- Build:
  - `cmake --build build -j$(( $(nproc) / 2 ))`
- Tests:
  - `ctest --test-dir build --output-on-failure`

## Local Workflow
1. `git checkout feature/tracy-profiler`
2. `git pull --rebase` (if remote branch is used)
3. `git checkout -b fix/<topic>`
4. Implement and validate with build + tests
5. Open PR to `feature/tracy-profiler`

## Safety Rules
- Do not force-push shared branches.
- Prefer merge only after local build/tests pass.
- Resolve stashes promptly: either convert into a branch+PR or drop them.
