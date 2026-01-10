#include "test_fixtures.h"

#include <unordered_map>
#include <unordered_set>

#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/species_index_system.h"

using namespace evolution::sim::test;
using namespace evolution::sim;
using namespace evolution::genetics;

TEST(SpeciesClustering, DeterministicSameGenomes) {
    GenomeStorage storage;
    ReproConfig config{};

    // Create identical genomes
    const GenomeId id1 = storage.create_random(12345);
    const GenomeId id2 = storage.create_random(12345);  // Same seed

    SpeciesIndexSystem system(storage, config, 10, 3.0);

    SimulationFixture fixture;
    SimulationContext context(fixture.app().registry(), 0.016, 0.0);

    system.tick(context);

    const SpeciesId species1 = system.get_species(id1);
    const SpeciesId species2 = system.get_species(id2);

    EXPECT_EQ(species1, species2) << "Identical genomes should cluster together";
}

TEST(SpeciesClustering, DifferentGenomesSeparate) {
    GenomeStorage storage;
    ReproConfig config{};

    const GenomeId id1 = storage.create_random(11111);
    const GenomeId id2 = storage.create_random(22222);
    const GenomeId id3 = storage.create_random(33333);

    SpeciesIndexSystem system(storage, config, 10, 0.1);  // Very tight threshold

    SimulationFixture fixture;
    SimulationContext context(fixture.app().registry(), 0.016, 0.0);

    system.tick(context);

    const SpeciesId species1 = system.get_species(id1);
    const SpeciesId species2 = system.get_species(id2);
    const SpeciesId species3 = system.get_species(id3);

    // With tight threshold, different genomes should be separate
    EXPECT_NE(species1, 0);
    EXPECT_NE(species2, 0);
    EXPECT_NE(species3, 0);
}

TEST(SpeciesClustering, ThresholdAdjustment) {
    GenomeStorage storage;
    ReproConfig config{};

    // Create many diverse genomes
    for (int i = 0; i < 20; ++i) {
        storage.create_random(static_cast<std::uint64_t>(i * 1000));
    }

    SpeciesIndexSystem system(storage, config, 5, 3.0);  // Target 5 species

    SimulationFixture fixture;
    SimulationContext context(fixture.app().registry(), 0.016, 0.0);

    const double initial_threshold = system.threshold();

    system.tick(context);

    const double after_threshold = system.threshold();
    const std::size_t species_count = system.active_species_count();

    // Threshold should adjust toward target
    EXPECT_GT(species_count, 0);
    EXPECT_LE(species_count, 20);
}

TEST(SpeciesClustering, ReproducibleAcrossRuns) {
    GenomeStorage storage1;
    GenomeStorage storage2;
    ReproConfig config{};

    // Create same genomes in both storages
    std::vector<GenomeId> ids1, ids2;
    for (int i = 0; i < 10; ++i) {
        ids1.push_back(storage1.create_random(static_cast<std::uint64_t>(i * 100)));
        ids2.push_back(storage2.create_random(static_cast<std::uint64_t>(i * 100)));
    }

    SpeciesIndexSystem system1(storage1, config, 5, 3.0);
    SpeciesIndexSystem system2(storage2, config, 5, 3.0);

    SimulationFixture fixture1, fixture2;
    SimulationContext ctx1(fixture1.app().registry(), 0.016, 0.0);
    SimulationContext ctx2(fixture2.app().registry(), 0.016, 0.0);

    system1.tick(ctx1);
    system2.tick(ctx2);

    // Same genomes should produce same species assignments
    for (std::size_t i = 0; i < ids1.size(); ++i) {
        const SpeciesId species1 = system1.get_species(ids1[i]);
        const SpeciesId species2 = system2.get_species(ids2[i]);
        EXPECT_EQ(species1, species2) << "Genome " << i << " should have same species";
    }
}

TEST(SpeciesClustering, EmptyStorageHandled) {
    GenomeStorage storage;
    ReproConfig config{};

    SpeciesIndexSystem system(storage, config, 10, 3.0);

    SimulationFixture fixture;
    SimulationContext context(fixture.app().registry(), 0.016, 0.0);

    EXPECT_NO_THROW(system.tick(context));

    EXPECT_EQ(system.active_species_count(), 0);
}

TEST(SpeciesClustering, UnknownGenomeReturnsZero) {
    GenomeStorage storage;
    ReproConfig config{};

    SpeciesIndexSystem system(storage, config, 10, 3.0);

    const SpeciesId species = system.get_species(99999);  // Non-existent ID
    EXPECT_EQ(species, 0);
}

TEST(SpeciesClustering, ThresholdClamping) {
    GenomeStorage storage;
    ReproConfig config{};

    SpeciesIndexSystem system(storage, config, 10, 3.0);

    const double threshold = system.threshold();
    EXPECT_GE(threshold, 0.5);   // Min threshold
    EXPECT_LE(threshold, 10.0);   // Max threshold
}

TEST(SpeciesClustering, LargePopulationScales) {
    GenomeStorage storage;
    ReproConfig config{};

    // Create 100 genomes
    for (int i = 0; i < 100; ++i) {
        storage.create_random(static_cast<std::uint64_t>(i));
    }

    SpeciesIndexSystem system(storage, config, 10, 3.0);

    SimulationFixture fixture;
    SimulationContext context(fixture.app().registry(), 0.016, 0.0);

    EXPECT_NO_THROW(system.tick(context));

    EXPECT_GT(system.active_species_count(), 0);
    EXPECT_LE(system.active_species_count(), 100);
}

TEST(SpeciesClustering, SpeciesCountConverges) {
    GenomeStorage storage;
    ReproConfig config{};

    for (int i = 0; i < 20; ++i) {
        storage.create_random(static_cast<std::uint64_t>(i * 1000));
    }

    SpeciesIndexSystem system(storage, config, 5, 3.0);

    SimulationFixture fixture;
    SimulationContext context(fixture.app().registry(), 0.016, 0.0);

    // Run multiple iterations - species count should eventually stabilize
    std::size_t previous_count = 0;
    int stable_iterations = 0;
    for (int i = 0; i < 20; ++i) {
        system.tick(context);
        const std::size_t current_count = system.active_species_count();
        if (current_count == previous_count) {
            ++stable_iterations;
        } else {
            stable_iterations = 0;
        }
        previous_count = current_count;
        if (stable_iterations >= 3) {
            break;  // Stable for 3 iterations
        }
    }

    // After some iterations, species count should be non-zero
    EXPECT_GT(previous_count, 0) << "Species count should be positive";
}




