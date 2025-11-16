#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_ops.h"

using namespace evolution::genetics;

TEST(CompatibilityDistance, IdenticalGenomes) {
    GenomeStorage storage;
    const GenomeId id = storage.create_random(11111);

    const auto* genome = storage.get(id);
    ASSERT_NE(genome, nullptr);

    ReproConfig config{};
    const double dist = compatibility_distance(*genome, *genome, config);
    EXPECT_NEAR(dist, 0.0, 1e-6);
}

TEST(CompatibilityDistance, Symmetry) {
    GenomeStorage storage;
    const GenomeId id_a = storage.create_random(22222);
    const GenomeId id_b = storage.create_random(33333);

    const auto* genome_a = storage.get(id_a);
    const auto* genome_b = storage.get(id_b);
    ASSERT_NE(genome_a, nullptr);
    ASSERT_NE(genome_b, nullptr);

    ReproConfig config{};
    const double dist_ab = compatibility_distance(*genome_a, *genome_b, config);
    const double dist_ba = compatibility_distance(*genome_b, *genome_a, config);

    EXPECT_NEAR(dist_ab, dist_ba, 1e-6);
}

TEST(CompatibilityDistance, NonNegative) {
    GenomeStorage storage;
    const GenomeId id_a = storage.create_random(44444);
    const GenomeId id_b = storage.create_random(55555);

    const auto* genome_a = storage.get(id_a);
    const auto* genome_b = storage.get(id_b);
    ASSERT_NE(genome_a, nullptr);
    ASSERT_NE(genome_b, nullptr);

    ReproConfig config{};
    const double dist = compatibility_distance(*genome_a, *genome_b, config);
    EXPECT_GE(dist, 0.0);
}

TEST(CompatibilityDistance, DifferentBrainKinds) {
    GenomeStorage storage;
    const GenomeId id_mlp = storage.create_random(66666);
    const GenomeId id_neat = storage.create_random(77777);

    const auto* genome_mlp = storage.get(id_mlp);
    const auto* genome_neat = storage.get(id_neat);
    ASSERT_NE(genome_mlp, nullptr);
    ASSERT_NE(genome_neat, nullptr);

    // Force different brain kinds by checking
    if (genome_mlp->brain_kind() == genome_neat->brain_kind()) {
        // Create another NEAT if both are MLP
        const GenomeId id_neat2 = storage.create_random(88888);
        const auto* genome_neat2 = storage.get(id_neat2);
        if (genome_neat2 && genome_neat2->brain_kind() != genome_mlp->brain_kind()) {
            ReproConfig config{};
            const double dist = compatibility_distance(*genome_mlp, *genome_neat2, config);
            EXPECT_GT(dist, 1.0);  // Should have large penalty
        }
    } else {
        ReproConfig config{};
        const double dist = compatibility_distance(*genome_mlp, *genome_neat, config);
        EXPECT_GT(dist, 1.0);  // Should have large penalty
    }
}

