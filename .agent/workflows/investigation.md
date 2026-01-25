---
description: Workflow for investigating unfamiliar code or issues
---

# Investigation Workflow

// turbo-all

Use this workflow when you need to understand something unfamiliar before making changes.

## 1. Define the Question

Clearly state what you're trying to understand:
- "How does X work?"
- "Where is Y defined?"
- "Why does Z happen?"

## 2. Start With Entry Point

```bash
# Find main entry point
grep -rn "int main" --include="*.cpp"

# Or find the specific function/class
grep -rn "class ClassName" --include="*.h"
grep -rn "def function_name" --include="*.py"
```

## 3. Trace the Flow

Starting from entry point, follow the execution:
```bash
# Find where function is called
grep -rn "functionName(" --include="*.cpp"

# Find what it calls
grep -n "functionName" file.cpp | head -50
```

## 4. Read Tests

Tests show expected behavior:
```bash
# Find related tests
find . -name "*test*" | xargs grep -l "ClassName"

# Read test cases
cat tests/test_file.cpp | head -100
```

## 5. Build Understanding

As you learn, note:
- Key data structures
- Main algorithms
- Important invariants
- Surprising behaviors

## 6. Verify Understanding

Test your mental model:
```bash
# Add temporary logging to confirm flow
# Run and observe output

# Or use debugger
gdb ./program
(gdb) break suspected_function
(gdb) run
```

## 7. Document Findings

If investigation reveals non-obvious behavior:
- Add comments to clarify
- Update documentation
- Consider if tests should document this

## 8. Answer Original Question

Circle back to your original question:
- Can you now answer it?
- If not, what's still unclear?
- Iterate if needed

## Quick Investigation Commands

```bash
# Project overview
tree -L 2 -d

# File structure
wc -l **/*.cpp **/*.h | sort -n | tail -20

# Recent changes
git log --oneline -20
git log --name-only -5

# Who changed this
git log -5 --follow path/to/file

# Find definition
grep -rn "class Thing\|struct Thing" --include="*.h"

# Find usage
grep -rn "Thing" --include="*.cpp" | grep -v "class Thing"

# Check includes
head -30 file.cpp | grep "#include"
```
