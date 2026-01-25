---
name: Build Systems
description: Handle common build toolchains - CMake, Make, npm, cargo, pip
---

# Build Systems Skill

How to configure, build, and troubleshoot common build systems.

## CMake (C/C++)

### Basic Workflow

```bash
# Create build directory (out-of-source build)
mkdir -p build && cd build

# Configure
cmake ..

# Build
cmake --build . 
# OR
make -j$(nproc)

# Install (optional)
cmake --install . --prefix /usr/local
```

### Common Options

```bash
# Set build type
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake -DCMAKE_BUILD_TYPE=Release ..

# Set compiler
cmake -DCMAKE_CXX_COMPILER=clang++ ..

# Set install prefix
cmake -DCMAKE_INSTALL_PREFIX=/opt/myapp ..

# Enable/disable options (project-specific)
cmake -DBUILD_TESTS=ON -DBUILD_EXAMPLES=OFF ..

# Verbose build
cmake --build . --verbose
```

### Presets (Modern CMake)

```bash
# List available presets
cmake --list-presets

# Configure with preset
cmake --preset debug

# Build with preset
cmake --build --preset debug
```

### Troubleshooting CMake

| Problem | Solution |
|---------|----------|
| "Could NOT find Package" | `apt install libpackage-dev` or set `Package_DIR` |
| Old CMake version | Update: `pip install cmake --upgrade` |
| Wrong compiler | Set `CC` and `CXX` env vars |
| Stale cache | Delete `build/CMakeCache.txt` or entire `build/` |
| Link errors | Check `target_link_libraries()` in CMakeLists.txt |

```bash
# Clean rebuild
rm -rf build && mkdir build && cd build && cmake .. && make
```

## Make

### Basic Usage

```bash
# Build default target
make

# Build specific target
make target_name

# Parallel build
make -j8
make -j$(nproc)

# Build with verbose output
make VERBOSE=1

# Clean build artifacts
make clean

# Install
make install
```

### Common Patterns

```bash
# Dry run (show commands without executing)
make -n

# Ignore errors and continue
make -k

# Rebuild specific file
make file.o

# Check if anything needs rebuilding
make -q && echo "Up to date" || echo "Needs rebuild"
```

### Troubleshooting Make

| Problem | Solution |
|---------|----------|
| "No rule to make target" | Check file exists, check Makefile paths |
| Undefined reference | Missing library in link step |
| Nothing to rebuild | Touch source file or clean first |
| Circular dependency | Check Makefile dependencies |

## npm / yarn (JavaScript)

### Basic Workflow

```bash
# Install dependencies
npm install
# OR
yarn

# Run scripts (defined in package.json)
npm run build
npm run dev
npm test

# Install specific package
npm install package-name
npm install -D package-name  # Dev dependency

# Update packages
npm update
npm outdated  # Check for updates
```

### Common Scripts

```bash
npm start       # Start application
npm run dev     # Development mode
npm run build   # Production build
npm test        # Run tests
npm run lint    # Run linter
```

### Troubleshooting npm

| Problem | Solution |
|---------|----------|
| "Module not found" | `npm install` or check import path |
| Version conflicts | Delete `node_modules` and `package-lock.json`, reinstall |
| Permission errors | Don't use sudo, fix npm prefix |
| Build fails | Check Node version: `node --version` |

```bash
# Nuclear option - clean reinstall
rm -rf node_modules package-lock.json
npm install
```

## Cargo (Rust)

### Basic Workflow

```bash
# Create new project
cargo new project_name

# Build
cargo build           # Debug
cargo build --release # Release

# Run
cargo run
cargo run --release

# Test
cargo test

# Check (faster than build)
cargo check
```

### Common Options

```bash
# Build with specific features
cargo build --features "feature1,feature2"

# Show build output
cargo build -v

# Update dependencies
cargo update

# Format code
cargo fmt

# Lint code
cargo clippy
```

## Python (pip / poetry / venv)

### Virtual Environments

```bash
# Create virtual environment
python -m venv venv

# Activate
source venv/bin/activate

# Deactivate
deactivate
```

### pip

```bash
# Install from requirements.txt
pip install -r requirements.txt

# Install package
pip install package-name

# Install in development mode
pip install -e .

# Freeze dependencies
pip freeze > requirements.txt

# Upgrade package
pip install --upgrade package-name
```

### Poetry

```bash
poetry install          # Install dependencies
poetry add package      # Add dependency
poetry run python app.py  # Run with poetry env
poetry shell            # Activate shell
poetry build            # Build package
```

## Universal Troubleshooting

### When Build Fails

1. **Read the FIRST error** - Later errors cascade from earlier ones
2. **Check dependencies** - Are they installed? Right version?
3. **Clean rebuild** - Delete build artifacts, start fresh
4. **Check environment** - Right compiler/interpreter version?
5. **Check permissions** - Can you write to build directory?

### Clean Build Commands

```bash
# CMake
rm -rf build && mkdir build && cd build && cmake .. && make

# npm
rm -rf node_modules package-lock.json && npm install

# Python
rm -rf __pycache__ *.pyc && pip install -r requirements.txt

# Cargo
cargo clean && cargo build
```

### Environment Verification

```bash
# Check versions
cmake --version
make --version
gcc --version
clang --version
node --version
npm --version
python --version
cargo --version
rustc --version
```
