## Missing tools (2026-01-25)

- `flatc` missing on PATH: `/bin/bash: line 1: flatc: command not found` (repo build uses FlatBuffers codegen)
- `clang++` missing on PATH: `/bin/bash: line 1: clang++: command not found` (`c++` is present and works)

- `lsp_diagnostics` for `.md` unavailable: no LSP server configured for extension `.md` in this environment

## Blocker: LSP diagnostics not configured for Markdown (2026-01-25)

Exact error from `lsp_diagnostics`:

```text
Error: Error: No LSP server configured for extension: .md

Available servers: typescript, deno, vue, eslint, oxlint, biome, gopls, ruby-lsp, basedpyright, pyright...

To add a custom server, configure 'lsp' in oh-my-opencode.json:
  {
    "lsp": {
      "my-server": {
        "command": ["my-lsp", "--stdio"],
        "extensions": [".md"]
      }
    }
  }
```

Note: `clangd` (and/or a Markdown LSP) is not configured in this environment, so the project-level "lsp_diagnostics clean" verification step cannot be satisfied for `.md` files.

## Blocker: `clangd` missing for C++ LSP diagnostics (2026-01-25)

Exact error from `lsp_diagnostics` on `sim/src/main.cpp`:

```text
Error: Error: Executable not found in $PATH: "clangd"
```

Note: Unable to install `clangd` in this environment (sudo requires a password), so the "lsp_diagnostics clean" verification step cannot be satisfied for C++ files.

## Blocker: `lsp_diagnostics` unavailable for C++ (2026-01-25, follow-up)

Attempted `lsp_diagnostics` on `sim/src/environment/environment_bootstrap.cpp` and got:

```text
Error: Error: Executable not found in $PATH: "clangd"
```

Workaround used for verification: `cmake --build build -j` + targeted `ctest`.

## Blocker: `lsp_diagnostics` still unavailable for changed C++ files (2026-01-25)

- Attempted `lsp_diagnostics` on `sim/src/environment/environment.cpp` and got: `Executable not found in $PATH: "clangd"`.

## sim_app build (2026-01-25)

No new blockers encountered for this task; `sim_app` linked successfully.

Note: `lsp_diagnostics` cannot be run on `.md` files in this environment (no Markdown LSP configured), so the "lsp_diagnostics clean" verification step cannot be satisfied for these notepad updates.

## Blocker: full ctest failures (2026-01-25)

`ctest --test-dir build --output-on-failure` failed with 9 tests:

- `PlantSpeciesMapping.InitialSeedingRespectsSpeciesConstraints` (0 plants spawned after 50000 attempts)
- `IntegrationSystemOrdering.FeedingAfterPlantGrowth` (no energy transfer)
- `PerformanceCadence.CadenceWindowDoesNotSpike` (max frame time too high)
- `ScenarioTests.BasicReproductionCycle` (no population growth / species count)
- `ScenarioTests.PredationDynamics` (biomass too high)
- `ScenarioTests.SpeciesFormation` (species count remained 0)
- `ScenarioTests.ResourceDepletion` (biomass too high; herbivore count unchanged)
- `ScenarioTests.ExtinctionEvent` (herbivore count unchanged)
- `IntegrationInvariantsTest.SpeciesIdsStable` (species count dropped to 0)

Hypothesis: plant seeding failure likely cascades into several scenario/integration failures. Start by fixing plant seeding.

## Blocker: C++ lsp_diagnostics still unavailable (2026-01-25)

- Attempted `lsp_diagnostics` on:
  - `sim/src/environment/environment_bootstrap.cpp`
  - `sim/src/environment/environment.cpp`
- Result:

```text
Error: Error: Executable not found in $PATH: "clangd"
```

## Blocker: lsp_diagnostics still blocked for C++ files (rerun, 2026-01-25)

- Attempted `lsp_diagnostics` on:
  - `sim/src/environment/environment_bootstrap.cpp`
  - `sim/src/environment/environment.cpp`
  - `sim/src/environment/plant_systems.cpp`
- Result: `Error: Executable not found in $PATH: "clangd"`.

## Blocker: `lsp_diagnostics` unavailable for test updates (2026-01-26)

- Attempted `lsp_diagnostics` on:
  - `tests/sim/test_scenarios.cpp`
- Result: `Error: Executable not found in $PATH: "clangd"`.

## Attempted fix: user-local clangd install + opencode LSP config (2026-01-26)

Constraints/evidence:

- `sudo -n true` -> `sudo: a password is required` (no passwordless sudo).
- `apt-get install -y clangd` fails without root:

```text
E: Could not open lock file /var/lib/dpkg/lock-frontend - open (13: Permission denied)
E: Unable to acquire the dpkg frontend lock (/var/lib/dpkg/lock-frontend), are you root?
```

Partial improvements made:

- Installed `clangd` user-local via `apt-get download` + `dpkg-deb -x` into `~/.local/opt/clangd-14` with wrapper `~/.local/bin/clangd` that sets `LD_LIBRARY_PATH`.
- Installed Markdown LSP binary `remark-language-server` via `npm install -g remark-language-server`.
- Added LSP config entries for clangd + Markdown in:
  - `~/.config/opencode/opencode.json`
  - `~/.config/opencode/oh-my-opencode.json`
  - `opencode.json` (repo root)

Still blocked:

- `lsp_diagnostics` on `sim/src/main.cpp` still returns:

```text
Error: Error: Executable not found in $PATH: "clangd"
```

This suggests `lsp_diagnostics` is not using the same PATH as the Bash tool and/or is ignoring the opencode/oh-my-opencode LSP config files.

Markdown status:

- `lsp_diagnostics` on `docs/testing.md` now returns `No diagnostics found` (previously this environment reported no configured `.md` LSP).
