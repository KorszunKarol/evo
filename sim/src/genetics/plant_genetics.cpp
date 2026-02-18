#include "evolution/genetics/plant_genetics.h"

#include <stdexcept>

#include <spdlog/spdlog.h>

namespace evolution::genetics {

std::unique_ptr<::evolution::genome::PlantTraitsT> extract_plant_traits(
    const ::evolution::genome::GenomeT genome) noexcept {
    if (genome == nullptr) {
        spdlog::warn("extract_plant_traits: null genome");
        return nullptr;
    }

    if (!genome->plant_traits()) {
        spdlog::warn("extract_plant_traits: genome lacks plant_traits field");
        return nullptr;
    }

    auto traits = std::make_unique<::evolution::genome::PlantTraitsT>();

    const double growth_rates[] = {2.0, 1.5, 2.5, 1.0, 2.0};
    const double max_energies[] = {20.0, 25.0, 15.0, 18.0, 22.0};
    const double seed_intervals[] = {20.0, 30.0, 15.0, 40.0, 22.0};
    const double seed_radii[] = {6.0, 5.0, 7.0, 4.0, 6.5};
    const double establish_probs[] = {0.65, 0.55, 0.70, 0.50, 0.60};

    const std::uint8_t idx = std::clamp(species_type, std::uint8_t{0}, std::uint8_t{4});

    evolution::genome::GenomeT genome;
    genome.version = 1;
    genome.generation = 0;
    genome.rng_seed = seed;
    genome.plant_traits = std::make_unique<::evolution::genome::PlantTraitsT>();

    auto& traits = *(genome.plant_traits);
    traits.growth_rate = growth_rates[idx];
    traits.max_energy = max_energies[idx];
    traits.seed_interval = seed_intervals[idx];
    traits.seed_radius = seed_radii[idx];
    traits.establish_prob = establish_probs[idx];

    const double min_energy = std::max(1.0, traits.max_energy * 0.6);
    const double seed_cost = std::max(0.5, traits.max_energy * 0.25);

    traits.seed_min_energy = min_energy;
    traits.seed_cost = seed_cost;

    GenomeStorage storage;
    const evolution::genetics::GenomeT genome_id = storage.insert(std::move(genome));

    spdlog::trace("created base plant genome: id={} species={}", genome_id, idx);

    return genome;
}

}

GenomeT create_base_plant_genome(std::uint64_t seed, std::uint8_t species_type) {
    Pcg32 rng(seed);

    const double growth_rates[] = {2.0, 1.5, 2.5, 1.0, 2.0};
    const double max_energies[] = {20.0, 25.0, 15.0, 18.0, 22.0};
    const double seed_intervals[] = {20.0, 30.0, 15.0, 40.0, 22.0};
    const double seed_radii[] = {6.0, 5.0, 7.0, 4.0, 6.5};
    const double establish_probs[] = {0.65, 0.55, 0.70, 0.50, 0.60};

    const std::uint8_t idx = std::clamp(species_type, std::uint8_t{0}, std::uint8_t{4});

    ::evolution::genome::GenomeT genome;
    genome.version = 1;
    genome.generation = 0;
    genome.rng_seed = seed;
    genome.plant_traits = std::make_unique<::evolution::genome::PlantTraitsT>();

    auto& traits = *(genome.plant_traits);
    traits.growth_rate = growth_rates[idx];
    traits.max_energy = max_energies[idx];
    traits.seed_interval = seed_intervals[idx];
    traits.seed_radius = seed_radii[idx];
    traits.establish_prob = establish_probs[idx];

    const double min_energy = std::max(1.0, traits.max_energy * 0.6);
    const double seed_cost = std::max(0.5, traits.max_energy * 0.25);

    traits.seed_min_energy = min_energy;
    traits.seed_cost = seed_cost;

    GenomeStorage storage;
    const GenomeId genome_id = storage.insert(std::move(genome));

    spdlog::trace("created base plant genome: id={} species={}", genome_id, idx);

    return genome_id;
}

}  // namespace evolution::genetics
