#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/trait_extraction.h"

using namespace evolution::genetics;

TEST(TraitExtraction, ExtractTraitVector) {
    GenomeStorage storage;
    const GenomeId id = storage.create_random(12345);

    const auto* genome = storage.get(id);
    ASSERT_NE(genome, nullptr);

    const auto traits = ExtractTraitVector(*genome);

    // All traits should be in [0, 1] range
    for (const double t : traits) {
        EXPECT_GE(t, 0.0);
        EXPECT_LE(t, 1.0);
    }
}

TEST(TraitExtraction, TraitCosineDistance) {
    std::array<double, 8> a{0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    std::array<double, 8> b{0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};

    const double dist = TraitCosineDistance(a, b);
    EXPECT_NEAR(dist, 0.0, 1e-6);  // Identical vectors

    std::array<double, 8> c{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    const double dist_ac = TraitCosineDistance(a, c);
    EXPECT_GE(dist_ac, 0.0);
    EXPECT_LE(dist_ac, 2.0);
}

TEST(TraitExtraction, Determinism) {
    GenomeStorage storage;
    const GenomeId id = storage.create_random(99999);

    const auto* genome = storage.get(id);
    ASSERT_NE(genome, nullptr);

    const auto traits1 = ExtractTraitVector(*genome);
    const auto traits2 = ExtractTraitVector(*genome);

    for (std::size_t i = 0; i < traits1.size(); ++i) {
        EXPECT_DOUBLE_EQ(traits1[i], traits2[i]);
    }
}

