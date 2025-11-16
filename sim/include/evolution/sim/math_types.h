#pragma once

#include <cmath>
#include <ostream>

namespace evolution::sim {

///
/// @brief Simple three-component vector tailored for simulation math utilities.
///
/// @details Provides basic arithmetic operations required for low-overhead physics integration.
/// @note Uses double precision to preserve numeric headroom during accumulation.
/// @threadsafe Reentrant as long as instances are not shared across threads without synchronization.
///
struct Vec3 {
    /// @brief X component in world units.
    double x{0.0};
    /// @brief Y component in world units.
    double y{0.0};
    /// @brief Z component in world units.
    double z{0.0};

    ///
    /// @brief Default constructor initializing a zero vector.
    ///
    constexpr Vec3() = default;

    ///
    /// @brief Constructs a vector from three scalar values.
    ///
    /// @param x_ double X axis component.
    /// @param y_ double Y axis component.
    /// @param z_ double Z axis component.
    ///
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    ///
    /// @brief Accumulates another vector component-wise.
    ///
    /// @param rhs const Vec3& Right-hand side operand.
    /// @return Vec3& Reference to this vector after modification.
    /// @complexity O(1)
    ///
    Vec3& operator+=(const Vec3& rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    ///
    /// @brief Subtracts another vector component-wise.
    ///
    /// @param rhs const Vec3& Right-hand side operand.
    /// @return Vec3& Reference to this vector after modification.
    /// @complexity O(1)
    ///
    Vec3& operator-=(const Vec3& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    ///
    /// @brief Scales the vector uniformly by a scalar value.
    ///
    /// @param scalar double Multiplicative factor.
    /// @return Vec3& Reference to this vector after modification.
    /// @complexity O(1)
    /// @warning Large scalar values can cause floating-point overflow.
    ///
    Vec3& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    ///
    /// @brief Computes the squared Euclidean length of the vector.
    ///
    /// @return double Squared magnitude in world units squared.
    /// @complexity O(1)
    ///
    [[nodiscard]] double length_squared() const {
        return x * x + y * y + z * z;
    }

    ///
    /// @brief Computes the Euclidean length of the vector.
    ///
    /// @return double Magnitude in world units.
    /// @complexity O(1)
    ///
    [[nodiscard]] double length() const {
        return std::sqrt(length_squared());
    }
};

///
/// @brief Returns the sum of two vectors.
///
/// @param lhs Vec3 Left-hand operand copied by value.
/// @param rhs const Vec3& Right-hand operand referenced to avoid copy.
/// @return Vec3 Resultant vector containing component-wise sum.
/// @complexity O(1)
///
inline Vec3 operator+(Vec3 lhs, const Vec3& rhs) {
    lhs += rhs;
    return lhs;
}

///
/// @brief Returns the difference between two vectors.
///
/// @param lhs Vec3 Left-hand operand copied by value.
/// @param rhs const Vec3& Right-hand operand referenced to avoid copy.
/// @return Vec3 Resultant vector containing component-wise difference.
/// @complexity O(1)
///
inline Vec3 operator-(Vec3 lhs, const Vec3& rhs) {
    lhs -= rhs;
    return lhs;
}

///
/// @brief Produces a vector scaled by a scalar value.
///
/// @param lhs Vec3 Vector operand copied by value.
/// @param scalar double Uniform scaling factor.
/// @return Vec3 Scaled vector result.
/// @complexity O(1)
///
inline Vec3 operator*(Vec3 lhs, double scalar) {
    lhs *= scalar;
    return lhs;
}

///
/// @brief Produces a vector scaled by a scalar value (commutative form).
///
/// @param scalar double Uniform scaling factor.
/// @param rhs Vec3 Vector operand copied by value.
/// @return Vec3 Scaled vector result.
/// @complexity O(1)
///
inline Vec3 operator*(double scalar, Vec3 rhs) {
    rhs *= scalar;
    return rhs;
}

///
/// @brief Streams a vector in human-readable form.
///
/// @param os std::ostream& Output stream instance.
/// @param v const Vec3& Vector to stream.
/// @return std::ostream& Reference to the provided output stream.
/// @complexity O(1)
///
inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    os << "Vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
    return os;
}

}  // namespace evolution::sim
