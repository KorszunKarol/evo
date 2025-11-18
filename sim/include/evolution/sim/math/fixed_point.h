#pragma once

#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <ostream>

namespace evolution::sim::math {

///
/// @brief Deterministic 64-bit fixed-point number (Q32.32).
///
/// @details Provides consistent arithmetic across platforms to ensure simulation determinism.
///          Uses 32 bits for the integer part and 32 bits for the fractional part.
///          Range: ~ +/- 2.14 billion.
///          Precision: ~ 2.32e-10.
///
/// @threadsafe Trivially copyable and thread-safe.
///
class Fixed64 {
public:
    using RawType = std::int64_t;
    static constexpr std::int32_t FractionalBits = 32;
    static constexpr RawType One = static_cast<RawType>(1) << FractionalBits;

    /// @brief Default constructor initializes to zero.
    constexpr Fixed64() = default;

    /// @brief Constructs from a raw underlying integer representation.
    /// @param raw Raw fixed-point value.
    static constexpr Fixed64 from_raw(RawType raw) {
        Fixed64 f;
        f.value_ = raw;
        return f;
    }

    /// @brief Constructs from an integer.
    constexpr Fixed64(std::int32_t v) : value_(static_cast<RawType>(v) << FractionalBits) {}
    constexpr Fixed64(std::int64_t v) : value_(v << FractionalBits) {}

    /// @brief Constructs from a float.
    constexpr Fixed64(float v)
        : value_(static_cast<RawType>(v * static_cast<float>(One))) {}

    /// @brief Constructs from a double.
    constexpr Fixed64(double v)
        : value_(static_cast<RawType>(v * static_cast<double>(One))) {}

    /// @brief Converts to double.
    [[nodiscard]] constexpr double to_double() const {
        return static_cast<double>(value_) / static_cast<double>(One);
    }

    /// @brief Converts to float.
    [[nodiscard]] constexpr float to_float() const {
        return static_cast<float>(value_) / static_cast<float>(One);
    }

    /// @brief Converts to integer (truncating towards zero).
    [[nodiscard]] constexpr std::int64_t to_int() const {
        return value_ / One;
    }

    /// @brief Returns the raw underlying representation.
    [[nodiscard]] constexpr RawType raw() const { return value_; }

    // --- Arithmetic Operators ---

    constexpr Fixed64 operator+(const Fixed64& rhs) const {
        return from_raw(value_ + rhs.value_);
    }

    constexpr Fixed64 operator-(const Fixed64& rhs) const {
        return from_raw(value_ - rhs.value_);
    }

    constexpr Fixed64 operator*(const Fixed64& rhs) const {
        // Multiply into 128-bit integer to avoid overflow, then shift back
        __int128 result = static_cast<__int128>(value_) * rhs.value_;
        return from_raw(static_cast<RawType>(result >> FractionalBits));
    }

    constexpr Fixed64 operator/(const Fixed64& rhs) const {
        if (rhs.value_ == 0) {
             // In simulation, we might want to handle this gracefully or trap.
             // For now, return max or zero? Let's let it trap/be undefined for speed or return 0 check.
             // To be safe/robust:
             return from_raw(0); 
        }
        __int128 numerator = static_cast<__int128>(value_) << FractionalBits;
        return from_raw(static_cast<RawType>(numerator / rhs.value_));
    }

    constexpr Fixed64& operator+=(const Fixed64& rhs) {
        value_ += rhs.value_;
        return *this;
    }

    constexpr Fixed64& operator-=(const Fixed64& rhs) {
        value_ -= rhs.value_;
        return *this;
    }

    constexpr Fixed64& operator*=(const Fixed64& rhs) {
        *this = *this * rhs;
        return *this;
    }

    constexpr Fixed64& operator/=(const Fixed64& rhs) {
        *this = *this / rhs;
        return *this;
    }

    constexpr Fixed64 operator-() const {
        return from_raw(-value_);
    }

    // --- Comparison Operators ---

    constexpr auto operator<=>(const Fixed64&) const = default;

    // --- Stream Operator ---
    friend std::ostream& operator<<(std::ostream& os, const Fixed64& f) {
        os << f.to_double();
        return os;
    }

private:
    RawType value_{0};
};

///
/// @brief Returns the absolute value.
///
inline constexpr Fixed64 abs(Fixed64 x) {
    return x.raw() < 0 ? -x : x;
}

}  // namespace evolution::sim::math

