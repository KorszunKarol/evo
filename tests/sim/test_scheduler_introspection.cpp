#include <gtest/gtest.h>

#include "evolution/sim/scheduler.h"

namespace evolution::sim {

namespace {

class TestSystem final : public ISystem {
public:
    explicit TestSystem(std::string_view name) noexcept : name_(name) {}

    void tick(SimulationContext&) override {}

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    std::string_view name_;
};

}  // namespace

TEST(Scheduler, ReturnsSystemNamesInOrder) {
    Scheduler scheduler;
    scheduler.add_system(std::make_unique<TestSystem>("alpha"));
    scheduler.add_system(std::make_unique<TestSystem>("beta"));
    scheduler.add_system(std::make_unique<TestSystem>("gamma"));

    const auto names = scheduler.system_names();

    ASSERT_EQ(names.size(), 3U);
    EXPECT_EQ(names[0], "alpha");
    EXPECT_EQ(names[1], "beta");
    EXPECT_EQ(names[2], "gamma");
}

}  // namespace evolution::sim
