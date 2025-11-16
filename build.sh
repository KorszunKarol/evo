#!/bin/bash
# Simple build script for the evolution simulation

set -e  # Exit on error

echo "🔨 Building Evolution Simulation..."

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure with CMake
echo "📋 Configuring project..."
cmake .. || {
    echo "❌ CMake configuration failed!"
    exit 1
}

# Build the project
echo "🔨 Compiling..."
cmake --build . -j$(nproc) || {
    echo "❌ Build failed!"
    exit 1
}

echo "✅ Build complete!"
echo ""
echo "To run the simulation:"
echo "  ./build/bin/sim_app"
echo ""
echo "Or from the build directory:"
echo "  cd build && ./bin/sim_app"

