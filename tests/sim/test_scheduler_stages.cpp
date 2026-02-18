#include <memory>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "evolution/sim/scheduler.h"

namespace evolution::sim {
namespace {

class RecordingSystem final : public ISystem {
public:
    RecordingSystem(int id, std::vector<int>* out) : id_(id), out_(out) {}

    void tick(SimulationContext&) override { out_->push_back(id_); }
    [[nodiscard]] std::string_view name() const override { return "recording"; }

private:
    int id_{0};
    std::vector<int>* out_{nullptr};
};

}  // namespace

TEST(SchedulerStages, StageOrderIsRespected) {
    entt::registry registry;
    SimulationContext context{registry, 1.0 / 60.0, 0.0};
    Scheduler scheduler;
    std::vector<int> order;

    scheduler.add_system(Scheduler::SystemStage::Metrics,
                         std::make_unique<RecordingSystem>(5, &order));
    scheduler.add_system(Scheduler::SystemStage::PrePhysics,
                         std::make_unique<RecordingSystem>(2, &order));
    scheduler.add_system(Scheduler::SystemStage::Physics,
                         std::make_unique<RecordingSystem>(4, &order));
    scheduler.add_system(Scheduler::SystemStage::Bootstrap,
                         std::make_unique<RecordingSystem>(1, &order));
    scheduler.add_system(std::make_unique<RecordingSystem>(3, &order));  // default Ecology
    scheduler.add_system(Scheduler::SystemStage::PostTick,
                         std::make_unique<RecordingSystem>(6, &order));

    scheduler.tick_systems(context);

    const std::vector<int> expected{1, 2, 3, 4, 5, 6};
    EXPECT_EQ(order, expected);
}

}  // namespace evolution::sim
