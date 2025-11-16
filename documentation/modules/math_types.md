# Module: Math Types

## Overview

The math types module provides fundamental mathematical data structures and operations used throughout the simulation. Currently implements a basic 3D vector type optimized for simulation physics.

## Files

- `sim/include/evolution/sim/math_types.h`

## Types

### Vec3

**Purpose**: Simple three-component vector tailored for simulation math utilities.

**Design Rationale**:
- **Double precision**: Preserves numeric headroom during accumulation
- **Minimal interface**: Only essential operations (no normalization, dot product yet)
- **Value semantics**: Copyable, movable, no dynamic allocation
- **Performance**: Inline functions, no virtual functions, cache-friendly

**Structure**:
```cpp
struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};
```

**Fields**:

- `x`: `double` - X-axis component
  - Units: World space meters
  - Default: `0.0`
  - Range: Any finite value

- `y`: `double` - Y-axis component
  - Units: World space meters
  - Default: `0.0`
  - Range: Any finite value

- `z`: `double` - Z-axis component
  - Units: World space meters
  - Default: `0.0`
  - Range: Any finite value

**Constructors**:

- **Default**: `Vec3()` → `{0.0, 0.0, 0.0}`
- **From components**: `Vec3(x, y, z)` → `{x, y, z}`

**Operations**:

- **Addition**: `vec1 += vec2`, `vec1 + vec2`
- **Subtraction**: `vec1 -= vec2`, `vec1 - vec2`
- **Scalar multiplication**: `vec *= scalar`, `vec * scalar`, `scalar * vec`
- **Length squared**: `vec.length_squared()` → `x² + y² + z²`
- **Length**: `vec.length()` → `√(x² + y² + z²)`

**Performance**:

- **Memory**: 24 bytes (3 × 8 bytes)
- **Alignment**: Natural alignment (8 bytes)
- **Operations**: All O(1)
- **Cache**: Single cache line (fits in 64-byte cache line)

**Usage Patterns**:

```cpp
// Construction
Vec3 position{10.0, 5.0, 0.0};
Vec3 velocity{0.0, -9.81, 0.0};

// Arithmetic
Vec3 acceleration = gravity + force;
Vec3 scaled = velocity * 2.0;

// Length calculation
double distance = position.length();
double distance_sq = position.length_squared();  // Faster (no sqrt)
```

**Thread Safety**:

- **Value type**: Each instance is independent
- **Operations**: Reentrant (no shared state)
- **Concurrent access**: Safe for concurrent reads, unsafe for concurrent writes to same instance

**Limitations**:

- **No normalization**: No `normalize()` or `normalized()` methods
- **No dot product**: No `dot()` method
- **No cross product**: No `cross()` method
- **No operators**: No `operator/`, `operator==`, etc.

**Future Extensions**:

- Normalization methods
- Dot and cross products
- Equality operators
- Stream operators (already has `operator<<`)
- SIMD optimizations (AVX2, AVX-512)

## Data Contracts

### Vec3 ↔ Components

**Contract**: Vec3 used in component fields.

**Usage**:
- `TransformComponent::position`: `Vec3` (world position)
- `KinematicsComponent::linear_velocity`: `Vec3` (velocity vector)
- `KinematicsComponent::accumulated_force`: `Vec3` (force vector)

**Guarantees**:
- Components always contain valid Vec3 instances
- Vec3 values are finite (no NaN, no Inf)
- Vec3 values represent physical quantities (meters, m/s, Newtons)

### Vec3 ↔ Physics System

**Contract**: Physics system uses Vec3 for all vector operations.

**Usage**:
- Gravity vector: `Vec3` constant
- Acceleration: `Vec3` computed from forces
- Velocity integration: `Vec3` updated each tick
- Position integration: `Vec3` updated each tick

**Operations**:
- Addition: `acceleration = gravity + accumulated_force`
- Scalar multiplication: `velocity += acceleration * dt`
- Component access: `position.y` for ground collision

**Guarantees**:
- All Vec3 operations preserve finite values
- No overflow checks (caller responsibility for large values)

## Related Documentation

- [Components Module](./modules/components.md) - Vec3 usage in components
- [Physics System Module](./modules/physics_system.md) - Vec3 usage in physics
- [API Reference](./api/function_reference.md#vec3) - Complete Vec3 API

