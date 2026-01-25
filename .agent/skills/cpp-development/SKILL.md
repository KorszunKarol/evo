---
name: C++ Development
description: C++ specific development workflows and debugging
---

# C++ Development Skill

C++-specific workflows, common errors, and debugging techniques.

## Project Build Workflow

### Standard CMake Project

```bash
# 1. Configure (from project root)
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 2. Build
cmake --build . -j$(nproc)

# 3. Run tests (if available)
ctest --output-on-failure

# 4. Run executable
./bin/program_name
```

### Build Types

```bash
# Debug - With debug symbols, no optimization
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release - Full optimization, no debug symbols
cmake -DCMAKE_BUILD_TYPE=Release ..

# RelWithDebInfo - Optimization + debug symbols
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
```

## Common Compiler Errors and Fixes

### Undefined Reference

```
undefined reference to `ClassName::method()'
```

**Causes:**
1. Missing source file in CMakeLists.txt
2. Missing `target_link_libraries()`
3. Method declared but not defined
4. Mismatched function signature

**Fix:**
```bash
# Check if symbol is defined somewhere
grep -rn "ClassName::method" --include="*.cpp"

# Check CMakeLists.txt for missing sources
cat CMakeLists.txt | grep -A 20 "add_executable\|add_library"
```

### No Matching Function

```
error: no matching function for call to 'func(int, string)'
```

**Causes:**
1. Wrong argument types
2. Missing `const` qualifier
3. Wrong number of arguments
4. Missing include

**Fix:**
```bash
# Find the function declaration
grep -rn "func(" --include="*.h"
# Compare expected signature with actual call
```

### Multiple Definition

```
multiple definition of `variable'
```

**Causes:**
1. Header defines variable, included in multiple translation units
2. Template specialization in header without `inline`

**Fix:**
- Use `extern` in header, define in one .cpp
- Use `inline` for functions in headers
- Use `static` or anonymous namespace for file-local

### Missing Include

```
error: 'vector' was not declared in this scope
```

**Fix:**
```cpp
#include <vector>       // Standard library
#include "myheader.h"   // Project header
```

### Include Guard Issues

Always use include guards:
```cpp
#ifndef MYHEADER_H
#define MYHEADER_H

// ... content ...

#endif // MYHEADER_H
```

Or pragma once:
```cpp
#pragma once
```

## Memory Debugging

### AddressSanitizer (Recommended)

```bash
# Configure with ASan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" ..

# Build and run - errors printed automatically
cmake --build . && ./program
```

**What ASan catches:**
- Use after free
- Heap/stack buffer overflow
- Memory leaks (with leak sanitizer)
- Double free

### Valgrind

```bash
# Basic memory check
valgrind ./program

# Full leak check
valgrind --leak-check=full --show-leak-kinds=all ./program

# With line numbers (requires debug build)
valgrind --leak-check=full --track-origins=yes ./program
```

### Undefined Behavior Sanitizer

```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=undefined -g" ..
```

**Catches:**
- Signed integer overflow
- Null pointer dereference
- Division by zero
- Invalid shift

## GDB for C++

### Basic Session

```bash
# Start debugging
gdb ./program

# Set breakpoint and run
(gdb) break main
(gdb) run arg1 arg2

# When it stops:
(gdb) bt                    # Backtrace
(gdb) print variable        # Print variable
(gdb) print *pointer        # Dereference
(gdb) print object          # Print object (needs operator<<)
(gdb) print object.member   # Print member
```

### Debugging Crashes

```bash
# Run until crash
(gdb) run

# After crash:
(gdb) bt full              # Full backtrace with locals
(gdb) frame 2              # Go to stack frame 2
(gdb) info locals          # Show local variables
(gdb) print this           # In method, print this pointer
```

### STL Container Debugging

GDB pretty-printers for STL (usually auto-enabled):
```bash
(gdb) print my_vector
# Shows: std::vector of length 3, capacity 4 = {1, 2, 3}

(gdb) print my_map
# Shows: std::map with 2 elements = {[1] = "one", [2] = "two"}
```

If not pretty-printed:
```bash
# Vector size
(gdb) print my_vector._M_impl._M_finish - my_vector._M_impl._M_start

# Vector element
(gdb) print *(my_vector._M_impl._M_start + 0)
```

### Useful GDB Commands

```bash
(gdb) watch variable        # Break when variable changes
(gdb) catch throw           # Break on throw
(gdb) catch catch           # Break on catch
(gdb) info threads          # List threads
(gdb) thread 2              # Switch to thread 2
(gdb) set print pretty on   # Pretty print structures
```

## Modern C++ Best Practices

### Smart Pointers

```cpp
// Prefer these over raw pointers
std::unique_ptr<Object> ptr = std::make_unique<Object>();
std::shared_ptr<Object> ptr = std::make_shared<Object>();
```

### RAII

```cpp
// Resources released automatically
{
    std::lock_guard<std::mutex> lock(mutex);  // Auto-unlocks
    std::unique_ptr<File> file = ...;          // Auto-closes
}  // Resources freed here
```

### Range-Based For

```cpp
for (const auto& item : container) {
    // Process item
}
```

### Nullptr vs NULL

```cpp
int* ptr = nullptr;  // Not NULL or 0
```

## Performance Profiling

### Quick Timing

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();
// ... code to measure ...
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
std::cout << "Time: " << duration.count() << "ms\n";
```

### perf (Linux)

```bash
# Profile
perf record ./program

# View results
perf report

# Flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

### Compiler Optimizations

```bash
# Check what optimizer is doing
g++ -O2 -S -fverbose-asm source.cpp  # Generate assembly

# Enable specific optimizations
-O2                  # Good balance
-O3                  # Aggressive
-march=native        # Use CPU-specific instructions
-flto                # Link-time optimization
```
