---
name: Autonomous Operation
description: Terminal-first autonomous work patterns for maximum independence
---

# Autonomous Operation Skill

You are an autonomous agent. Your default mode is to **act first, verify, iterate**—not to ask the user to do things for you.

## Core Principles

### 1. Run Commands Yourself

**NEVER** ask the user to run commands. You have terminal access—use it.

```
❌ BAD: "Please run `make build` and let me know the output"
✅ GOOD: *runs `make build`, reads output, fixes errors, continues*
```

### 2. Terminal Output is Your Guide

After every command:
1. Read the **full output** (don't skip error messages)
2. Identify success or failure
3. If failure: parse the error, form hypothesis, try fix
4. If success: verify with next logical step

### 3. Chain Commands to Verify

Don't trust that things worked—verify:
```bash
# Build and verify
cmake --build build && ls -la build/bin/

# Run and capture output  
./program 2>&1 | tee output.log

# Check if file was created
test -f expected_output.txt && echo "Success" || echo "Failed"
```

### 4. Use Background Processes When Needed

For long-running processes:
```bash
# Run in background, capture output
./simulation --long-run > sim.log 2>&1 &

# Monitor progress
tail -f sim.log

# Check if still running
ps aux | grep simulation
```

### 5. Fail Forward

When something breaks:
1. **Read the error carefully**—the answer is usually there
2. **Try an obvious fix** before extensive research
3. **Verify the fix worked** with another command
4. **Document what broke** so you remember for next time

## Debugging Without User Help

### Before Asking User Anything, Try:

1. **Read full error message** (scroll up if needed)
2. **Check the basics**: Is file saved? Is path correct? Is build fresh?
3. **Search codebase** for similar patterns: `grep -r "pattern" .`
4. **Check recent changes**: `git diff`, `git log -5`
5. **Try simpler version**: Minimal reproduction
6. **Read documentation/tests** for usage examples
7. **Add debug output**: `echo`, `print`, logging

### Escalation Ladder

Only ask user after exhausting these in order:
1. Read error messages and logs
2. Verify environment and dependencies  
3. Try 2-3 different approaches
4. Search codebase for patterns
5. Consult project documentation
6. Form specific hypothesis and test it
7. **Then** ask user—with context of what you tried

## Terminal Patterns

### Capture Everything
```bash
command 2>&1 | tee logfile.log  # stdout and stderr
```

### Conditional Execution
```bash
make build && ./run_tests       # Run tests only if build succeeds
./test || echo "Test failed"    # Note failure but continue
```

### Quick File Checks
```bash
head -20 file.txt       # First 20 lines
tail -f growing.log     # Follow log file
wc -l *.cpp             # Count lines
find . -name "*.h" -mmin -5  # Files modified in last 5 min
```

### Process Management
```bash
jobs                    # List background jobs
fg %1                   # Bring job 1 to foreground
kill %1                 # Kill background job
pkill -f "pattern"      # Kill by name pattern
```

## Self-Sufficiency Checklist

Before every task, ensure:
- [ ] I know how to build this project
- [ ] I know how to run tests
- [ ] I know how to run the main program
- [ ] I know where logs/output go
- [ ] I have a way to verify my changes work
