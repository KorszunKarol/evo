---
name: Code Exploration
description: Efficiently understand unfamiliar codebases
---

# Code Exploration Skill

How to quickly understand a codebase you've never seen before.

## Phase 1: Get the Lay of the Land (5 minutes)

### 1. Directory Structure

```bash
# Overview of project structure
tree -L 2 -d          # Directories only, 2 levels deep
ls -la                # Root level files

# Count files by type
find . -name "*.cpp" | wc -l
find . -name "*.py" | wc -l
find . -name "*.h" | wc -l
```

Look for standard patterns:
- `src/` or `lib/` → Source code
- `include/` or `headers/` → Public interfaces
- `tests/` or `test/` → Test files (great for usage examples)
- `docs/` or `documentation/` → Documentation
- `scripts/` → Build/utility scripts
- `config/` or `.config` → Configuration

### 2. Read Key Files

Priority order:
1. `README.md` - Project overview, build instructions
2. `CMakeLists.txt`, `package.json`, `Cargo.toml` - Build config, dependencies
3. `main.cpp`, `main.py`, `index.js` - Entry points
4. `ARCHITECTURE.md`, `DESIGN.md` - Design documents (if they exist)

### 3. Check Build System

```bash
# What's the build command?
cat Makefile | head -30
cat CMakeLists.txt | head -50
cat package.json | jq '.scripts'
```

## Phase 2: Trace the Entry Points (10 minutes)

### Find Main Entry Points

```bash
# C/C++
grep -rn "int main" --include="*.cpp"

# Python
grep -rn "if __name__" --include="*.py"

# JavaScript/TypeScript
grep -rn "module.exports\|export default" --include="*.js" --include="*.ts"
```

### Trace Execution Flow

From main, identify:
1. Initialization sequence
2. Main loop or request handler
3. Key subsystems being invoked

```bash
# Find function definitions
grep -rn "void functionName\|def functionName" .

# Find function calls
grep -rn "functionName(" .
```

## Phase 3: Understand Key Abstractions (15 minutes)

### Find Core Types/Classes

```bash
# C++ classes
grep -rn "^class " --include="*.h" --include="*.hpp"

# Python classes
grep -rn "^class " --include="*.py"

# Interfaces/Traits
grep -rn "interface\|trait\|protocol" .
```

### Map Dependencies

```bash
# What does this file include/import?
head -50 src/main.cpp | grep "#include"
head -30 main.py | grep "^import\|^from"

# What includes this file?
grep -rn "#include.*filename.h" .
```

### Read Tests for Usage Examples

Tests are living documentation. They show how code is meant to be used.

```bash
# Find test files
find . -name "*test*.cpp" -o -name "test_*.py" -o -name "*.test.js"

# Read test cases for a specific class
grep -A 30 "TEST.*ClassName" tests/
```

## Phase 4: Deep Dive into Specific Areas

### Trace a Feature

Pick a feature and trace it end-to-end:
1. Find UI/API entry point
2. Follow function calls down
3. Find data access layer
4. Understand the full flow

### Use Code Navigation

```bash
# Find all definitions of a symbol
grep -rn "^def symbol_name\|^class symbol_name\|symbol_name.*=" .

# Find all usages
grep -rn "symbol_name" . | grep -v "def symbol_name\|class symbol_name"

# Find related files
find . -name "*feature*" -type f
```

### Build Incrementally

Don't try to understand everything. Build mental model in layers:
1. High-level architecture (boxes and arrows)
2. Key data structures
3. Main algorithms/workflows
4. Edge cases and error handling

## Quick Reference Patterns

### "What does X do?"
```bash
view_file_outline path/to/file.cpp    # See structure
grep -B 5 -A 20 "function_name" file  # See implementation with context
```

### "Where is X defined?"
```bash
grep -rn "class X\|struct X\|def X\|function X" .
```

### "Where is X used?"
```bash
grep -rn "X(" . --include="*.cpp" | grep -v "class X\|def X"
```

### "What files changed recently?"
```bash
git log --oneline -20
git log --name-only -5
git diff HEAD~5 --name-only
```

### "How do I build/run this?"
```bash
cat README.md
cat Makefile
cat CMakeLists.txt | head -50
cat package.json | jq '.scripts'
```

## Anti-Patterns to Avoid

1. **Reading every file sequentially** - Use search, follow references
2. **Ignoring tests** - They're the best usage documentation
3. **Not building the mental model** - Draw boxes/arrows if needed
4. **Asking user before searching** - Search the codebase first
5. **Missing the entry point** - Always start from main/entrypoint
