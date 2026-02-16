## Unresolved: lsp_diagnostics cannot locate clangd (2026-01-26)

- Even after installing user-local `clangd` and configuring LSP in `opencode.json`, `lsp_diagnostics` still reports: `Executable not found in $PATH: "clangd"`.
- Likely cause: `lsp_diagnostics` tool runs with a restricted PATH and/or does not load global/project OpenCode LSP config in this environment.
