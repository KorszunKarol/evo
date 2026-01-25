---
name: Problem Solving
description: Systematic debugging and troubleshooting methodology
---

# Problem Solving Skill

A systematic approach to debugging and unblocking yourself.

## The Debugging Mindset

Debugging is **hypothesis-driven science**:
1. Observe the symptom
2. Form a hypothesis about the cause
3. Design an experiment to test it
4. Interpret results
5. Iterate

## Step-by-Step Debugging Process

### Step 1: Reproduce the Problem

**Never fix what you can't reproduce.**

```bash
# Run the failing command again, capture full output
./failing_command 2>&1 | tee debug.log

# Note the exact conditions
echo "Failed at $(date) with args: $*"
```

If you can't reproduce:
- Was it a transient failure? (network, race condition)
- Did state change? (files, environment)
- Is it environment-specific?

### Step 2: Read the Error Message—ALL of It

Most debugging failures happen because developers don't read carefully.

```
Look for:
- File paths and line numbers
- Function/method names in stack trace
- The ACTUAL error (often near the end)
- Suggestions in the error message itself
```

### Step 3: Isolate the Problem

**Binary search** to find the root cause:

```bash
# Which commit broke it?
git bisect start
git bisect bad HEAD
git bisect good known_good_commit

# Which file broke it?
# Comment out half, test, narrow down

# Which line broke it?
# Add debug prints at strategic points
```

### Step 4: Check the Obvious

Before diving deep, verify basics:

```bash
# Is the code actually compiled/saved?
ls -la build/bin/target
stat source_file.cpp

# Is the environment correct?
echo $PATH
which python
cat .env

# Are dependencies installed?
pip list | grep package
npm list

# Is the config correct?
cat config.yaml
```

### Step 5: Form and Test Hypotheses

Write down your hypothesis explicitly:

```
HYPOTHESIS: The crash happens because `creature_count` becomes negative
            when a creature dies during iteration.

TEST: Add assertion `assert(creature_count >= 0)` before the loop
      and run the simulation.

RESULT: [fill in after testing]
```

### Step 6: Use Proper Debugging Tools

**Don't just add print statements.** Use real debuggers:

#### GDB (C/C++)
```bash
gdb ./program
(gdb) break main
(gdb) run
(gdb) backtrace        # Show call stack
(gdb) print variable   # Inspect value
(gdb) step             # Step into
(gdb) next             # Step over
(gdb) continue         # Continue to next breakpoint
```

#### Python Debugger
```bash
python -m pdb script.py
# Or add in code:
# import pdb; pdb.set_trace()
```

#### Core Dumps
```bash
ulimit -c unlimited          # Enable core dumps
./crashing_program           # Let it crash
gdb ./program core           # Analyze core dump
(gdb) backtrace
```

### Step 7: Document What You Learn

After solving:
```bash
# Add comment explaining the fix
# Update documentation if it's a common issue
# Consider if a test should prevent regression
```

## Common Debugging Patterns

### Segmentation Faults
1. Run with AddressSanitizer: `cmake -DCMAKE_CXX_FLAGS="-fsanitize=address"`
2. Use Valgrind: `valgrind --leak-check=full ./program`
3. Check for null pointers, array bounds, use-after-free

### Build Failures
1. Read the FIRST error (later errors cascade)
2. Check include paths and library links
3. Clean rebuild: `rm -rf build && mkdir build && cmake ..`

### Test Failures
1. Run single failing test in isolation
2. Add debug output to see actual vs expected
3. Check test setup/teardown for state issues

### Performance Issues
1. Profile first: `perf record ./program && perf report`
2. Use flame graphs for visualization
3. Check algorithmic complexity, not just micro-optimizations

### "Works on My Machine"
1. Compare environments: `env`, `pip freeze`, compiler versions
2. Check for hardcoded paths
3. Verify all dependencies are committed/documented

## When to Ask for Help

Ask user ONLY after:
- [ ] You've read the full error message
- [ ] You've tried at least 2 different approaches
- [ ] You've searched the codebase for similar patterns
- [ ] You've consulted available documentation
- [ ] You can clearly articulate what you tried and what happened

When asking, provide:
- What you're trying to do
- What error/behavior you see
- What you've already tried
- Your best hypothesis for the cause
