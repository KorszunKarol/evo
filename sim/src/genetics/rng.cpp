#include "evolution/genetics/rng.h"

#include <cmath>
#include <limits>

namespace evolution::genetics {

namespace {
constexpr std::uint64_t kMultiplier = 6364136223846793005ULL;
constexpr double kTwoPi = 6.283185307179586476925286766559;  // 2π
}  // namespace

Pcg32::Pcg32(std::uint64_t seed, std::uint64_t stream) noexcept {
    reseed(seed, stream);
}

void Pcg32::reseed(std::uint64_t seed, std::uint64_t stream) noexcept {
    state_ = 0;
    increment_ = (stream << 1u) | 1u;
    has_cached_ = false;
    cached_value_ = 0.0;
    next_u32();
    state_ += seed;
    next_u32();
}

std::uint32_t Pcg32::next_u32() noexcept {
    const std::uint64_t oldstate = state_;
    state_ = oldstate * kMultiplier + increment_;
    const std::uint32_t xorshifted = static_cast<std::uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);
    const std::uint32_t rot = static_cast<std::uint32_t>(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

std::uint64_t Pcg32::next_u64() noexcept {
    const auto hi = static_cast<std::uint64_t>(next_u32()) << 32u;
    return hi | static_cast<std::uint64_t>(next_u32());
}

double Pcg32::next_unit() noexcept {
    constexpr double kDivisor = static_cast<double>(UINT64_C(1) << 53);
    return static_cast<double>(next_u64() >> 11u) / kDivisor;
}

double Pcg32::uniform(double min, double max) noexcept {
    return min + (max - min) * next_unit();
}

double Pcg32::normal(double mean, double stddev) noexcept {
    if (has_cached_) {
        has_cached_ = false;
        return mean + stddev * cached_value_;
    }

    double u1 = 0.0;
    double u2 = 0.0;
    do {
        u1 = next_unit();
    } while (u1 <= std::numeric_limits<double>::min());
    u2 = next_unit();

    const double mag = std::sqrt(-2.0 * std::log(u1));
    const double z0 = mag * std::cos(kTwoPi * u2);
    const double z1 = mag * std::sin(kTwoPi * u2);

    cached_value_ = z1;
    has_cached_ = true;

    return mean + stddev * z0;
}

void Pcg32::advance(std::uint64_t delta) noexcept {
    std::uint64_t cur_mult = kMultiplier;
    std::uint64_t cur_plus = increment_;
    std::uint64_t acc_mult = 1u;
    std::uint64_t acc_plus = 0u;

    while (delta > 0) {
        if (delta & 1u) {
            acc_mult *= cur_mult;
            acc_plus = acc_plus * cur_mult + cur_plus;
        }
        cur_plus = (cur_mult + 1u) * cur_plus;
        cur_mult *= cur_mult;
        delta >>= 1u;
    }

    state_ = acc_mult * state_ + acc_plus;
    has_cached_ = false;
}

std::uint64_t derive_seed(std::uint64_t global_seed,
                          std::uint64_t genome_seed,
                          std::uint32_t op_tag,
                          std::uint32_t counter) noexcept {
    // Combine inputs using XOR and bit mixing
    std::uint64_t combined = global_seed ^ genome_seed;
    combined ^= static_cast<std::uint64_t>(op_tag) << 32u;
    combined ^= static_cast<std::uint64_t>(counter);
    // Mix bits
    combined ^= combined >> 33u;
    combined *= 0xff51afd7ed558ccdULL;
    combined ^= combined >> 33u;
    combined *= 0xc4ceb9fe1a85ec53ULL;
    combined ^= combined >> 33u;
    return combined;
}

}  // namespace evolution::genetics


