---
description: Systematic workflow for debugging and fixing bugs
---

# Bug Fix Workflow

// turbo-all

## 1. Create Bug Fix Branch

```bash
git checkout -b fix/descriptive-bug-name
```

## 2. Reproduce the Bug

Before fixing anything, reproduce the issue:
```bash
# Run the failing scenario
./build/bin/program_name args

# Capture the output
./build/bin/program_name args 2>&1 | tee bug_output.log
```

If you can't reproduce:
- Check exact conditions (inputs, config, environment)
- Try different scenarios
- Verify it's not already fixed on main

## 3. Write Failing Test

Create a test that captures the bug:
```bash
# Add test case that currently fails
# This ensures the bug won't regress
```

Run the test to confirm it fails:
```bash
ctest -R "BugTestName" --output-on-failure
```

## 4. Isolate Root Cause

Use binary search approach:
```bash
# Add debug output to narrow down
# Use git bisect if needed
git bisect start
git bisect bad HEAD
git bisect good known_good_commit
```

Check common causes:
- Off-by-one errors
- Null/dangling pointers
- Race conditions
- Incorrect assumptions

## 5. Debug With Tools

For crashes:
```bash
# Run with GDB
gdb ./build/bin/program_name
(gdb) run
(gdb) bt  # when crashed
```

For memory issues:
```bash
# Rebuild with ASan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
cmake --build .
./build/bin/program_name
```

## 6. Implement Fix

Apply minimal fix:
- Don't refactor unrelated code
- Keep the fix focused
- Document why the fix works

## 7. Verify Fix

```bash
# Run the specific test
ctest -R "BugTestName" --output-on-failure

# Run related tests
ctest -R "RelatedPattern" --output-on-failure

# Run full suite to catch regressions
ctest --output-on-failure
```

## 8. Clean Up

Remove debug code:
```bash
git diff --staged  # Review changes
# Remove any temporary debugging
```

## 9. Commit

```bash
git add .
git commit -m "fix: describe what was fixed

Root cause: brief explanation
Solution: how it was fixed
Fixes #issue-number (if applicable)"
```

## 10. Push

```bash
git push -u origin fix/descriptive-bug-name
```
