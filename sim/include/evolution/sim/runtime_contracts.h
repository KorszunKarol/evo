#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

namespace evolution::sim {

struct RunConfig {
    std::uint64_t seed{2025};
    double fixed_dt{1.0 / 60.0};
    bool test_mode{false};
    std::string scenario_name{"default"};
};

struct HealthReport {
    bool build_ok{false};
    bool tests_ok{false};
    std::size_t failing_tests{0};
    std::vector<std::string> failed_test_names{};
};

class ISoilField {
public:
    virtual ~ISoilField() = default;
    [[nodiscard]] virtual double sample_nutrient(double x, double z) const noexcept = 0;
    virtual void consume_nutrient(double x, double z, double amount) noexcept = 0;
};

class IPlantIndex {
public:
    virtual ~IPlantIndex() = default;
    [[nodiscard]] virtual std::vector<entt::entity> query_radius(double x,
                                                                  double z,
                                                                  double radius) const = 0;
};

class IWorldTime {
public:
    virtual ~IWorldTime() = default;
    [[nodiscard]] virtual double fixed_dt() const noexcept = 0;
    [[nodiscard]] virtual double simulation_time() const noexcept = 0;
    [[nodiscard]] virtual std::size_t tick_count() const noexcept = 0;
};

}  // namespace evolution::sim
