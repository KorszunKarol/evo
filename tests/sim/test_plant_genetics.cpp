#include "evolution/genetics/plant_genetics.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/rng.h"
#include "evolution/genomics/genome_generated.h"

#include <gtest/gtest.h>

namespace evolution::sim::test {

#include "evolution/genetics/plant_genetics.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/rng.h"
#include "evolution/genomics/genome_generated.h"

#include <gtest/gtest.h>

namespace evolution::sim::test {

TEST(PlantGenetics, ExtractPlantTraits_ValidGenome) {
    evolution::genetics::GenomeStorage storage;

    evolution::genome::GenomeT genome;
    genome.version = 1;
    genome.id = 0;
    genome.generation = 0;
    genome.rng_seed = 12345;
    genome.plant_traits = std::make_unique<evolution::genome::PlantTraitsT>();

    auto& traits = *(genome.plant_traits);
    traits.growth_rate = 2.5f;
    traits.max_energy = 25.0f;
    traits.seed_interval = 20.0f;
    traits.seed_radius = 6.0f;
    traits.establish_prob = 0.65f;

    const GenomeId genome_id = storage.insert(std::move(genome));
    ASSERT_NE(genome_id, 0);

    const auto* retrieved_genome = storage.get(genome_id);
    ASSERT_NE(retrieved_genome, nullptr);

    auto extracted = evolution::genetics::extract_plant_traits(retrieved_genome);
    ASSERT_NE(extracted, nullptr);

    EXPECT_FLOAT_EQ(extracted->growth_rate, 2.5f);
    EXPECT_FLOAT_EQ(extracted->max_energy, 25.0f);
    EXPECT_FLOAT_EQ(extracted->seed_interval, 20.0f);
    EXPECT_FLOAT_EQ(extracted->seed_radius, 6.0f);
    EXPECT_FLOAT_EQ(extracted->establish_prob, 0.65f);
}

TEST(PlantGenetics, ExtractPlantTraits_NullGenome) {
    evolution::genetics::GenomeStorage storage;

    const auto extracted = evolution::genetics::extract_plant_traits(nullptr);
    EXPECT_EQ(extracted, nullptr);
}

TEST(PlantGenetics, ExtractPlantTraits_MissingPlantTraits) {
    evolution::genetics::GenomeStorage storage;

    evolution::genome::GenomeT genome;
    genome.version = 1;
    genome.id = 0;
    genome.generation = 0;
    genome.rng_seed = 12345;

    const GenomeId genome_id = storage.insert(std::move(genome));
    ASSERT_NE(genome_id, 0);

    const auto* retrieved_genome = storage.get(genome_id);
    ASSERT_NE(retrieved_genome, nullptr);

    auto extracted = evolution::genetics::extract_plant_traits(retrieved_genome);
    EXPECT_EQ(extracted, nullptr);
}

TEST(PlantGenetics, CreateBasePlantGenome_Species0_Plains) {
    evolution::genetics::GenomeStorage storage;

    const evolution::genome::GenomeT result = evolution::genetics::create_base_plant_genome(99999, 0);

    EXPECT_EQ(result.generation, 0);
    EXPECT_EQ(result.rng_seed, 99999);

    ASSERT_NE(result.plant_traits, nullptr);
    EXPECT_FLOAT_EQ(result.plant_traits->growth_rate, 2.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->max_energy, 20.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_interval, 20.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_radius, 6.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->establish_prob, 0.65f);
}

TEST(PlantGenetics, CreateBasePlantGenome_Species1_Forest) {
    evolution::genetics::GenomeStorage storage;

    const evolution::genome::GenomeT result = evolution::genetics::create_base_plant_genome(88888, 1);

    EXPECT_EQ(result.generation, 0);
    EXPECT_EQ(result.rng_seed, 88888);

    ASSERT_NE(result.plant_traits, nullptr);
    EXPECT_FLOAT_EQ(result.plant_traits->growth_rate, 1.5f);
    EXPECT_FLOAT_EQ(result.plant_traits->max_energy, 25.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_interval, 30.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_radius, 5.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->establish_prob, 0.55f);
}

TEST(PlantGenetics, CreateBasePlantGenome_Species2_Wetland) {
    evolution::genetics::GenomeStorage storage;

    const evolution::genome::GenomeT result = evolution::genetics::create_base_plant_genome(77777, 2);

    EXPECT_EQ(result.generation, 0);
    EXPECT_EQ(result.rng_seed, 77777);

    ASSERT_NE(result.plant_traits, nullptr);
    EXPECT_FLOAT_EQ(result.plant_traits->growth_rate, 2.5f);
    EXPECT_FLOAT_EQ(result.plant_traits->max_energy, 15.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_interval, 15.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_radius, 7.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->establish_prob, 0.70f);
}

TEST(PlantGenetics, CreateBasePlantGenome_Species3_Alpine) {
    evolution::genetics::GenomeStorage storage;

    const evolution::genome::GenomeT result = evolution::genetics::create_base_plant_genome(66666, 3);

    EXPECT_EQ(result.generation, 0);
    EXPECT_EQ(result.rng_seed, 66666);

    ASSERT_NE(result.plant_traits, nullptr);
    EXPECT_FLOAT_EQ(result.plant_traits->growth_rate, 1.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->max_energy, 18.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_interval, 40.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_radius, 4.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->establish_prob, 0.50f);
}

TEST(PlantGenetics, CreateBasePlantGenome_Species4_Generalist) {
    evolution::genetics::GenomeStorage storage;

    const evolution::genome::GenomeT result = evolution::genetics::create_base_plant_genome(55555, 4);

    EXPECT_EQ(result.generation, 0);
    EXPECT_EQ(result.rng_seed, 55555);

    ASSERT_NE(result.plant_traits, nullptr);
    EXPECT_FLOAT_EQ(result.plant_traits->growth_rate, 2.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->max_energy, 22.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_interval, 22.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->seed_radius, 6.5f);
    EXPECT_FLOAT_EQ(result.plant_traits->establish_prob, 0.60f);
}

TEST(PlantGenetics, CreateBasePlantGenome_InvalidSpeciesIndex) {
    evolution::genetics::GenomeStorage storage;

    const evolution::genome::GenomeT result = evolution::genetics::create_base_plant_genome(44444, 5);

    EXPECT_EQ(result.generation, 0);
    EXPECT_EQ(result.rng_seed, 44444);

    ASSERT_NE(result.plant_traits, nullptr);
    EXPECT_FLOAT_EQ(result.plant_traits->growth_rate, 2.0f);
    EXPECT_FLOAT_EQ(result.plant_traits->max_energy, 22.0f);
}

}  // namespace evolution::sim::test
