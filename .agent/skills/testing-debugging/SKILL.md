---
name: Testing and Debugging
description: Terminal-based test execution and debugging workflows
---

# Testing and Debugging Skill

Run tests, interpret failures, and debug issues—all from the terminal.

## Running Tests

### C++ (CTest/GoogleTest)

```bash
# Build tests first
cmake --build build --target all

# Run all tests
cd build && ctest --output-on-failure

# Run with verbose output
ctest -V

# Run specific test
ctest -R "TestName"

# Run tests matching pattern
ctest -R "Creature.*"

# List available tests
ctest -N

# Run with parallelism
ctest -j8
```

For GoogleTest binaries directly:
```bash
# Run all tests
./build/bin/test_executable

# Run specific test
./build/bin/test_executable --gtest_filter="*TestName*"

# List tests
./build/bin/test_executable --gtest_list_tests

# Run with output
./build/bin/test_executable --gtest_output=json:results.json
```

### Python (pytest)

```bash
# Run all tests
pytest

# Run with verbose output
pytest -v

# Run specific file
pytest tests/test_module.py

# Run specific test function
pytest tests/test_module.py::test_function

# Run tests matching pattern
pytest -k "pattern"

# Stop on first failure
pytest -x

# Show print statements
pytest -s

# Run with coverage
pytest --cov=src --cov-report=html

# Run failed tests from last run
pytest --lf
```

### JavaScript/TypeScript (Jest/Mocha)

```bash
# Jest
npm test
npm test -- --watch
npm test -- --testNamePattern="pattern"
npm test -- --coverage

# Mocha
npx mocha
npx mocha --grep "pattern"
npx mocha --watch
```

### Rust (cargo)

```bash
cargo test
cargo test test_name
cargo test -- --nocapture  # Show println
cargo test -- --test-threads=1  # Sequential
```

## Interpreting Test Failures

### Read the Full Output

Don't just look at "FAILED". Read:
1. **Which test failed** - Name tells you what was being tested
2. **Assertion message** - What was expected vs actual
3. **Stack trace** - Where exactly did it fail
4. **Setup context** - What state was the test in

### Common Failure Patterns

| Pattern | Meaning | Fix |
|---------|---------|-----|
| Expected X, got Y | Logic error | Check algorithm |
| Segfault in test | Memory issue | Run with ASan |
| Timeout | Infinite loop or slow | Add debug prints, profile |
| Flaky (sometimes passes) | Race condition | Check test isolation |
| Fails in CI, passes locally | Environment issue | Check dependencies |

### Debugging a Failing Test

```bash
# 1. Run just the failing test
ctest -R "FailingTest" -V

# 2. Run with debugger (C++)
gdb ./build/bin/test_binary
(gdb) break TestFixture::FailingTest
(gdb) run --gtest_filter="*FailingTest*"

# 3. Run with debugger (Python)
python -m pytest tests/test_file.py::test_name --pdb

# 4. Add debug output to test
# Edit test, add prints, run again
```

## Debugging With GDB

### Basic Session

```bash
gdb ./program
(gdb) run arg1 arg2         # Start program
(gdb) bt                    # Backtrace when crashed
(gdb) frame 3               # Go to stack frame 3
(gdb) print variable        # Print variable value
(gdb) print *pointer        # Dereference pointer
(gdb) quit
```

### Set Breakpoints

```bash
(gdb) break main                    # Break at function
(gdb) break file.cpp:42             # Break at line
(gdb) break MyClass::method         # Break at method
(gdb) break file.cpp:42 if x > 10   # Conditional break
(gdb) info breakpoints              # List breakpoints
(gdb) delete 1                      # Delete breakpoint 1
```

### Stepping

```bash
(gdb) next      # Step over (don't enter functions)
(gdb) step      # Step into function
(gdb) finish    # Continue until function returns
(gdb) continue  # Continue to next breakpoint
```

### Inspect State

```bash
(gdb) info locals           # All local variables
(gdb) print sizeof(object)  # Size of object
(gdb) ptype variable        # Type of variable
(gdb) x/10x pointer         # Examine 10 hex words at address
```

## Memory Debugging

### AddressSanitizer (ASan)

```bash
# Build with ASan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
cmake --build .

# Run - will report memory errors with stack traces
./program
```

ASan catches:
- Use after free
- Buffer overflows
- Memory leaks
- Double free

### Valgrind

```bash
# Run with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./program

# With more detail
valgrind --track-origins=yes ./program
```

### ThreadSanitizer (for race conditions)

```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..
```

## Continuous Testing

### Watch Mode

```bash
# Python - pytest-watch
pip install pytest-watch
ptw

# JavaScript - Jest
npm test -- --watch

# Manual watch with entr
find . -name "*.cpp" | entr -c make test
```

### Test-Driven Workflow

1. Write failing test first
2. Run test, confirm it fails
3. Write minimal code to pass
4. Run test, confirm it passes
5. Refactor
6. Run test, confirm still passes

## Quick Reference

| Task | Command |
|------|---------|
| Run all tests | `ctest` / `pytest` / `npm test` |
| Run one test | `ctest -R Name` / `pytest path::test` |
| Verbose output | `ctest -V` / `pytest -v` |
| Stop on failure | `ctest --stop-on-failure` / `pytest -x` |
| Debug test | `gdb ./test --gtest_filter=Name` |
| Coverage | `pytest --cov` / `lcov` |
