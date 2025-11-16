#include "test_fixtures.h"

#include <unordered_set>

#include "evolution/genetics/rng.h"

using namespace evolution::sim::test;
using namespace evolution::genetics;
using GenomeId = evolution::genetics::GenomeId;

TEST(RngSeedDerivation, DeterministicSameInputs) {
    const std::uint64_t global_seed = 12345;
    const std::uint64_t genome_seed = 67890;
    const std::uint32_t op_tag = 0x524550524F;  // REPRO
    const std::uint32_t counter = 42;

    const std::uint64_t seed1 = derive_seed(global_seed, genome_seed, op_tag, counter);
    const std::uint64_t seed2 = derive_seed(global_seed, genome_seed, op_tag, counter);

    EXPECT_EQ(seed1, seed2) << "Same inputs must produce same seed";
}

TEST(RngSeedDerivation, DifferentOpTagsProduceDifferentSeeds) {
    const std::uint64_t global_seed = 12345;
    const std::uint64_t genome_seed = 67890;
    const std::uint32_t counter = 42;

    const std::uint64_t seed_repro = derive_seed(global_seed, genome_seed, 0x524550524F, counter);
    const std::uint64_t seed_cross = derive_seed(global_seed, genome_seed, 0x43524F5353, counter);
    const std::uint64_t seed_mutate = derive_seed(global_seed, genome_seed, 0x4D555441, counter);

    EXPECT_NE(seed_repro, seed_cross);
    EXPECT_NE(seed_repro, seed_mutate);
    EXPECT_NE(seed_cross, seed_mutate);
}

TEST(RngSeedDerivation, DifferentCountersProduceDifferentSeeds) {
    const std::uint64_t global_seed = 12345;
    const std::uint64_t genome_seed = 67890;
    const std::uint32_t op_tag = 0x524550524F;

    std::unordered_set<std::uint64_t> seeds;
    for (std::uint32_t counter = 0; counter < 100; ++counter) {
        const std::uint64_t seed = derive_seed(global_seed, genome_seed, op_tag, counter);
        EXPECT_EQ(seeds.count(seed), 0) << "Counter " << counter << " produced duplicate seed";
        seeds.insert(seed);
    }
}

TEST(RngSeedDerivation, DifferentGenomeSeedsProduceDifferentSeeds) {
    const std::uint64_t global_seed = 12345;
    const std::uint32_t op_tag = 0x524550524F;
    const std::uint32_t counter = 42;

    std::unordered_set<std::uint64_t> seeds;
    for (std::uint64_t genome_seed = 1; genome_seed < 100; ++genome_seed) {
        const std::uint64_t seed = derive_seed(global_seed, genome_seed, op_tag, counter);
        EXPECT_EQ(seeds.count(seed), 0) << "Genome seed " << genome_seed << " produced duplicate";
        seeds.insert(seed);
    }
}

TEST(RngSeedDerivation, PairKeyStability) {
    const std::uint64_t global_seed = 12345;
    const std::uint32_t op_tag = 0x524550524F;

    // Test that min/max ordering produces same seed
    const std::uint64_t parent_a = 100;
    const std::uint64_t parent_b = 200;
    const std::uint64_t pair_key1 = std::min(parent_a, parent_b) ^ std::max(parent_a, parent_b);
    const std::uint64_t pair_key2 = std::min(parent_b, parent_a) ^ std::max(parent_b, parent_a);

    EXPECT_EQ(pair_key1, pair_key2);

    const std::uint64_t seed1 = derive_seed(global_seed, pair_key1, op_tag, 0);
    const std::uint64_t seed2 = derive_seed(global_seed, pair_key2, op_tag, 0);
    EXPECT_EQ(seed1, seed2);
}

TEST(RngSeedDerivation, ZeroSeedsHandled) {
    const std::uint64_t seed1 = derive_seed(0, 0, 0, 0);
    const std::uint64_t seed2 = derive_seed(0, 0, 0, 0);
    EXPECT_EQ(seed1, seed2);
}

TEST(RngSeedDerivation, LargeSeedsHandled) {
    const std::uint64_t global_seed = UINT64_MAX;
    const std::uint64_t genome_seed = UINT64_MAX - 1;
    const std::uint32_t op_tag = UINT32_MAX;
    const std::uint32_t counter = UINT32_MAX;

    const std::uint64_t seed1 = derive_seed(global_seed, genome_seed, op_tag, counter);
    const std::uint64_t seed2 = derive_seed(global_seed, genome_seed, op_tag, counter);
    EXPECT_EQ(seed1, seed2);
}

TEST(RngSeedDerivation, CadenceIndexIncrements) {
    const std::uint64_t global_seed = 12345;
    const std::uint64_t genome_seed = 67890;
    const std::uint32_t op_tag = 0x524550524F;

    std::vector<std::uint64_t> seeds;
    for (std::uint32_t cadence = 0; cadence < 10; ++cadence) {
        seeds.push_back(derive_seed(global_seed, genome_seed, op_tag, cadence));
    }

    // All seeds should be different
    std::unordered_set<std::uint64_t> unique_seeds(seeds.begin(), seeds.end());
    EXPECT_EQ(unique_seeds.size(), seeds.size());
}

