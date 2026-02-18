## LSP enablement approach (2026-01-26)

- Chosen approach: user-local installs for missing language servers (no sudo available).
- Rationale: `sudo` prompts for password in this environment, and `apt-get install` cannot acquire dpkg locks as non-root.
- Implementation:
  - `clangd` installed via Ubuntu `.deb` extraction into `~/.local/opt/clangd-14` + wrapper `~/.local/bin/clangd` to supply required shared libs via `LD_LIBRARY_PATH`.
  - Markdown LSP via `remark-language-server` installed with npm.
  - Added `lsp` configuration to `~/.config/opencode/opencode.json`, `~/.config/opencode/oh-my-opencode.json`, and repo `opencode.json`.
