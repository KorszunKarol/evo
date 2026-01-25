---
description: Workflow for implementing new features
---

# New Feature Workflow

// turbo-all

## 1. Create Feature Branch

```bash
git checkout -b feature/your-feature-name
```

## 2. Explore Related Code

Before writing any code, understand the existing patterns:
```bash
# Find related files
grep -rn "related_keyword" --include="*.cpp" --include="*.h"

# Check existing tests for usage patterns
find . -name "*test*" -type f | xargs grep -l "related"
```

## 3. Write Tests First

Create or update test file:
```bash
# Create test file if needed
touch tests/test_feature.cpp
```

Write tests that describe expected behavior:
- Happy path tests
- Edge case tests
- Error handling tests

## 4. Implement Feature

Write the implementation following SOLID principles:
- Single responsibility - one thing per function
- Use interfaces for flexibility
- Keep functions small and focused

## 5. Build and Test

```bash
cd build
cmake --build . -j$(nproc)
ctest --output-on-failure -R "TestPattern"
```

If tests fail:
- Read error message carefully
- Debug with GDB if needed
- Fix and re-run

## 6. Run Full Test Suite

```bash
ctest --output-on-failure
```

## 7. Check Code Quality

```bash
# Check for compiler warnings
cmake --build . 2>&1 | grep -i warning

# Run with sanitizers if applicable
# cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" ..
```

## 8. Update Documentation

- Add docstrings to new functions
- Update README if needed
- Add usage examples

## 9. Commit

Only commit if all tests pass:
```bash
git add .
git status
git commit -m "feat: implement your-feature-name

- Describe what was added
- Note any breaking changes"
```

## 10. Cleanup

```bash
# Verify commit
git log -1

# Push when ready
git push -u origin feature/your-feature-name
```
