#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_ops.h"

using namespace evolution::genetics;

TEST(Mutation, Determinism) {
    GenomeStorage storage;
    const GenomeId id = storage.create_random(123456);

    const auto* genome = storage.get(id);
    ASSERT_NE(genome, nullptr);

    auto genome_obj = genome->UnPack();
    const std::uint64_t seed = 999999;

    ReproConfig config{};
    auto mutated1 = mutate(*genome_obj, config, seed);
    genome_obj = genome->UnPack();  // Reset
    auto mutated2 = mutate(*genome_obj, config, seed);

    // Same seed should produce same mutation
    EXPECT_EQ(mutated1.brain_kind, mutated2.brain_kind);
    if (mutated1.mlp && mutated2.mlp) {
        EXPECT_EQ(mutated1.mlp->weights.size(), mutated2.mlp->weights.size());
        for (std::size_t i = 0; i < mutated1.mlp->weights.size(); ++i) {
            EXPECT_FLOAT_EQ(mutated1.mlp->weights[i], mutated2.mlp->weights[i]);
        }
    }
}

TEST(Crossover, Determinism) {
    GenomeStorage storage;
    const GenomeId id_a = storage.create_random(111111);
    const GenomeId id_b = storage.create_random(222222);

    const auto* genome_a = storage.get(id_a);
    const auto* genome_b = storage.get(id_b);
    ASSERT_NE(genome_a, nullptr);
    ASSERT_NE(genome_b, nullptr);

    auto obj_a = genome_a->UnPack();
    auto obj_b = genome_b->UnPack();
    const std::uint64_t seed = 333333;

    ReproConfig config{};
    auto child1 = crossover(*obj_a, *obj_b, config, seed);
    obj_a = genome_a->UnPack();  // Reset
    obj_b = genome_b->UnPack();
    auto child2 = crossover(*obj_a, *obj_b, config, seed);

    // Same parents + seed should produce same child
    EXPECT_EQ(child1.brain_kind, child2.brain_kind);
    EXPECT_EQ(child1.generation, child2.generation);
}

TEST(Crossover, ParentIds) {
    GenomeStorage storage;
    const GenomeId id_a = storage.create_random(444444);
    const GenomeId id_b = storage.create_random(555555);

    const auto* genome_a = storage.get(id_a);
    const auto* genome_b = storage.get(id_b);
    ASSERT_NE(genome_a, nullptr);
    ASSERT_NE(genome_b, nullptr);

    auto obj_a = genome_a->UnPack();
    auto obj_b = genome_b->UnPack();

    ReproConfig config{};
    auto child = crossover(*obj_a, *obj_b, config, 666666);

    EXPECT_EQ(child.parents.size(), 2u);
    EXPECT_EQ(child.parents[0], id_a);
    EXPECT_EQ(child.parents[1], id_b);
    EXPECT_GT(child.generation, std::max(obj_a->generation, obj_b->generation));
}

TEST(Mutation, MutatorGene) {
    GenomeStorage storage;
    const GenomeId id = storage.create_random(777777);

    const auto* genome = storage.get(id);
    ASSERT_NE(genome, nullptr);

    auto genome_obj = genome->UnPack();

    // Add mutator gene
    genome_obj->mutator = std::make_unique<evolution::genome::MutatorT>();
    genome_obj->mutator->mutate_rate_struct = 0.1f;
    genome_obj->mutator->mutate_rate_param = 0.3f;
    genome_obj->mutator->weight_sigma = 0.25f;

    ReproConfig default_config{};
    auto mutated = mutate(std::move(*genome_obj), default_config, 888888);

    // Mutator gene should be preserved
    ASSERT_NE(mutated.mutator, nullptr);
    EXPECT_FLOAT_EQ(mutated.mutator->mutate_rate_struct, 0.1f);
}

