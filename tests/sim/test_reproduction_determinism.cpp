#include "test_fixtures.h"

#include <unordered_set>

#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/phenotype_builder.h"
#include "evolution/sim/reproduction_system.h"

using namespace evolution::sim::test;
using namespace evolution::sim;
using namespace evolution::genetics;

TEST(ReproductionDeterminism, SameSeedSameOffspring) {
    SimulationFixture fixture1, fixture2;
    auto& registry1 = fixture1.app().registry();
    auto& registry2 = fixture2.app().registry();

    const std::uint64_t global_seed = 12345;
    ReproConfig config{};

    // Create identical parent genomes
    const GenomeId parent_a_id1 = fixture1.create_test_genome(11111);
    const GenomeId parent_b_id1 = fixture1.create_test_genome(22222);
    const GenomeId parent_a_id2 = fixture2.create_test_genome(11111);
    const GenomeId parent_b_id2 = fixture2.create_test_genome(22222);

    const entt::entity parent_a1 = fixture1.spawn_herbivore(Vec3{0, 0, 0}, parent_a_id1);
    const entt::entity parent_b1 = fixture1.spawn_herbivore(Vec3{1, 0, 0}, parent_b_id1);
    const entt::entity parent_a2 = fixture2.spawn_herbivore(Vec3{0, 0, 0}, parent_a_id2);
    const entt::entity parent_b2 = fixture2.spawn_herbivore(Vec3{1, 0, 0}, parent_b_id2);

    // Set up reproduction components
    auto& repro_a1 = registry1.get<ReproductionComponent>(parent_a1);
    auto& repro_b1 = registry1.get<ReproductionComponent>(parent_b1);
    repro_a1.timer = 0.0;
    repro_b1.timer = 0.0;
    repro_a1.energy_threshold = 50.0;
    repro_b1.energy_threshold = 50.0;

    auto& metab_a1 = registry1.get<MetabolismComponent>(parent_a1);
    auto& metab_b1 = registry1.get<MetabolismComponent>(parent_b1);
    metab_a1.energy = 100.0;
    metab_b1.energy = 100.0;

    auto& repro_a2 = registry2.get<ReproductionComponent>(parent_a2);
    auto& repro_b2 = registry2.get<ReproductionComponent>(parent_b2);
    repro_a2.timer = 0.0;
    repro_b2.timer = 0.0;
    repro_a2.energy_threshold = 50.0;
    repro_b2.energy_threshold = 50.0;

    auto& metab_a2 = registry2.get<MetabolismComponent>(parent_a2);
    auto& metab_b2 = registry2.get<MetabolismComponent>(parent_b2);
    metab_a2.energy = 100.0;
    metab_b2.energy = 100.0;

    ReproductionSystem repro_system1(fixture1.storage(), config, global_seed);
    ReproductionSystem repro_system2(fixture2.storage(), config, global_seed);

    SimulationContext ctx1(registry1, 0.016, 0.0);
    SimulationContext ctx2(registry2, 0.016, 0.0);

    repro_system1.tick(ctx1);
    repro_system2.tick(ctx2);

    // Count offspring
    auto view1 = registry1.view<GenomeHandleComponent>();
    auto view2 = registry2.view<GenomeHandleComponent>();

    std::unordered_set<GenomeId> offspring_ids1, offspring_ids2;
    for (auto entity : view1) {
        if (entity != parent_a1 && entity != parent_b1) {
            const auto& handle = view1.get<GenomeHandleComponent>(entity);
            offspring_ids1.insert(handle.id);
        }
    }
    for (auto entity : view2) {
        if (entity != parent_a2 && entity != parent_b2) {
            const auto& handle = view2.get<GenomeHandleComponent>(entity);
            offspring_ids2.insert(handle.id);
        }
    }

    EXPECT_EQ(offspring_ids1.size(), offspring_ids2.size())
        << "Same seed should produce same number of offspring";
}

TEST(ReproductionDeterminism, EnergyCostDeducted) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const std::uint64_t global_seed = 12345;
    ReproConfig config{};

    const GenomeId parent_a_id = fixture.create_test_genome(11111);
    const GenomeId parent_b_id = fixture.create_test_genome(22222);

    const entt::entity parent_a = fixture.spawn_herbivore(Vec3{0, 0, 0}, parent_a_id);
    const entt::entity parent_b = fixture.spawn_herbivore(Vec3{1, 0, 0}, parent_b_id);

    auto& repro_a = registry.get<ReproductionComponent>(parent_a);
    auto& repro_b = registry.get<ReproductionComponent>(parent_b);
    repro_a.timer = 0.0;
    repro_b.timer = 0.0;
    repro_a.energy_threshold = 50.0;
    repro_b.energy_threshold = 50.0;

    auto& metab_a = registry.get<MetabolismComponent>(parent_a);
    auto& metab_b = registry.get<MetabolismComponent>(parent_b);
    const double initial_energy_a = 100.0;
    const double initial_energy_b = 100.0;
    metab_a.energy = initial_energy_a;
    metab_b.energy = initial_energy_b;

    ReproductionSystem repro_system(fixture.storage(), config, global_seed);
    SimulationContext context(registry, 0.016, 0.0);

    const std::size_t initial_entities = registry.storage<entt::entity>().in_use();

    repro_system.tick(context);

    // Energy should be deducted
    EXPECT_LT(metab_a.energy, initial_energy_a);
    EXPECT_LT(metab_b.energy, initial_energy_b);

    // Offspring should be created
    EXPECT_GT(registry.storage<entt::entity>().in_use(), initial_entities);
}

TEST(ReproductionDeterminism, CooldownRespected) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const std::uint64_t global_seed = 12345;
    ReproConfig config{};

    const GenomeId parent_a_id = fixture.create_test_genome(11111);
    const GenomeId parent_b_id = fixture.create_test_genome(22222);

    const entt::entity parent_a = fixture.spawn_herbivore(Vec3{0, 0, 0}, parent_a_id);
    const entt::entity parent_b = fixture.spawn_herbivore(Vec3{1, 0, 0}, parent_b_id);

    auto& repro_a = registry.get<ReproductionComponent>(parent_a);
    auto& repro_b = registry.get<ReproductionComponent>(parent_b);
    repro_a.timer = 0.0;
    repro_b.timer = 0.0;
    repro_a.energy_threshold = 50.0;
    repro_b.energy_threshold = 50.0;

    auto& metab_a = registry.get<MetabolismComponent>(parent_a);
    auto& metab_b = registry.get<MetabolismComponent>(parent_b);
    metab_a.energy = 100.0;
    metab_b.energy = 100.0;

    ReproductionSystem repro_system(fixture.storage(), config, global_seed);

    const std::size_t initial_count = registry.storage<entt::entity>().in_use();

    // First reproduction
    SimulationContext context1(registry, 0.016, 0.0);
    repro_system.tick(context1);

    const std::size_t after_first = registry.storage<entt::entity>().in_use();
    EXPECT_GT(after_first, initial_count);

    // Immediately try again - should be blocked by cooldown
    SimulationContext context2(registry, 0.016, 0.016);
    repro_system.tick(context2);

    const std::size_t after_second = registry.storage<entt::entity>().in_use();
    EXPECT_EQ(after_second, after_first) << "Cooldown should prevent immediate reproduction";
}

TEST(ReproductionDeterminism, InsufficientEnergyBlocks) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const std::uint64_t global_seed = 12345;
    ReproConfig config{};

    const GenomeId parent_a_id = fixture.create_test_genome(11111);
    const GenomeId parent_b_id = fixture.create_test_genome(22222);

    const entt::entity parent_a = fixture.spawn_herbivore(Vec3{0, 0, 0}, parent_a_id);
    const entt::entity parent_b = fixture.spawn_herbivore(Vec3{1, 0, 0}, parent_b_id);

    auto& repro_a = registry.get<ReproductionComponent>(parent_a);
    auto& repro_b = registry.get<ReproductionComponent>(parent_b);
    repro_a.timer = 0.0;
    repro_b.timer = 0.0;
    repro_a.energy_threshold = 100.0;
    repro_b.energy_threshold = 100.0;

    auto& metab_a = registry.get<MetabolismComponent>(parent_a);
    auto& metab_b = registry.get<MetabolismComponent>(parent_b);
    metab_a.energy = 50.0;  // Below threshold
    metab_b.energy = 50.0;  // Below threshold

    ReproductionSystem repro_system(fixture.storage(), config, global_seed);
    SimulationContext context(registry, 0.016, 0.0);

    const std::size_t initial_count = registry.storage<entt::entity>().in_use();

    repro_system.tick(context);

    EXPECT_EQ(registry.storage<entt::entity>().in_use(), initial_count)
        << "Insufficient energy should block reproduction";
}

TEST(ReproductionDeterminism, OffspringHasGenomeHandle) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const std::uint64_t global_seed = 12345;
    ReproConfig config{};

    const GenomeId parent_a_id = fixture.create_test_genome(11111);
    const GenomeId parent_b_id = fixture.create_test_genome(22222);

    const entt::entity parent_a = fixture.spawn_herbivore(Vec3{0, 0, 0}, parent_a_id);
    const entt::entity parent_b = fixture.spawn_herbivore(Vec3{1, 0, 0}, parent_b_id);

    auto& repro_a = registry.get<ReproductionComponent>(parent_a);
    auto& repro_b = registry.get<ReproductionComponent>(parent_b);
    repro_a.timer = 0.0;
    repro_b.timer = 0.0;
    repro_a.energy_threshold = 50.0;
    repro_b.energy_threshold = 50.0;

    auto& metab_a = registry.get<MetabolismComponent>(parent_a);
    auto& metab_b = registry.get<MetabolismComponent>(parent_b);
    metab_a.energy = 100.0;
    metab_b.energy = 100.0;

    ReproductionSystem repro_system(fixture.storage(), config, global_seed);
    SimulationContext context(registry, 0.016, 0.0);

    repro_system.tick(context);

    // Find offspring
    auto view = registry.view<GenomeHandleComponent>();
    for (auto entity : view) {
        if (entity != parent_a && entity != parent_b) {
            const auto& handle = view.get<GenomeHandleComponent>(entity);
            EXPECT_NE(handle.id, 0) << "Offspring should have valid genome ID";
            EXPECT_NE(handle.id, parent_a_id);
            EXPECT_NE(handle.id, parent_b_id);
        }
    }
}

TEST(ReproductionDeterminism, OffspringCountIncremented) {
    SimulationFixture fixture;
    auto& registry = fixture.app().registry();

    const std::uint64_t global_seed = 12345;
    ReproConfig config{};

    const GenomeId parent_a_id = fixture.create_test_genome(11111);
    const GenomeId parent_b_id = fixture.create_test_genome(22222);

    const entt::entity parent_a = fixture.spawn_herbivore(Vec3{0, 0, 0}, parent_a_id);
    const entt::entity parent_b = fixture.spawn_herbivore(Vec3{1, 0, 0}, parent_b_id);

    auto& fitness_a = registry.get<FitnessComponent>(parent_a);
    auto& fitness_b = registry.get<FitnessComponent>(parent_b);
    const std::uint32_t initial_offspring_a = fitness_a.offspring_count;
    const std::uint32_t initial_offspring_b = fitness_b.offspring_count;

    auto& repro_a = registry.get<ReproductionComponent>(parent_a);
    auto& repro_b = registry.get<ReproductionComponent>(parent_b);
    repro_a.timer = 0.0;
    repro_b.timer = 0.0;
    repro_a.energy_threshold = 50.0;
    repro_b.energy_threshold = 50.0;

    auto& metab_a = registry.get<MetabolismComponent>(parent_a);
    auto& metab_b = registry.get<MetabolismComponent>(parent_b);
    metab_a.energy = 100.0;
    metab_b.energy = 100.0;

    ReproductionSystem repro_system(fixture.storage(), config, global_seed);
    SimulationContext context(registry, 0.016, 0.0);

    repro_system.tick(context);

    EXPECT_GT(fitness_a.offspring_count, initial_offspring_a);
    EXPECT_GT(fitness_b.offspring_count, initial_offspring_b);
}



